#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/tpl.hpp>

#include <atomic>
#include <chrono>
#include <vector>
#include <mutex>
#include <string>
#include <type_traits>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading::tasks;

namespace {

struct IWorker {
    virtual ~IWorker() = default;
    virtual int step(int x) = 0;
    virtual void on_cancel() = 0;
};

// Hand-written recording mock (FakeIt's vtable tricks crash/hang under newer GCC and sanitizers).
struct RecordingWorker final : IWorker {
    int step(int x) override {
        std::lock_guard lock(mtx);
        step_args.push_back(x);
        return x + 1;
    }
    void on_cancel() override {
        std::lock_guard lock(mtx);
        ++cancel_calls;
    }

    std::mutex mtx;
    std::vector<int> step_args;
    int cancel_calls = 0;
};

} // namespace

TEST_CASE("CancellationToken::none is never canceled", "[cancellation]") {
    auto token = CancellationToken::none();
    REQUIRE_FALSE(token.is_cancellation_requested());
    REQUIRE_NOTHROW(token.throw_if_cancellation_requested());
    REQUIRE_NOTHROW(token.register_callback([] {}));
}

TEST_CASE("CancellationTokenSource handles manual cancel, idempotency, and late callbacks", "[cancellation]") {
    CancellationTokenSource cts;
    auto token = cts.token();

    std::atomic<int> callback_hits{0};
    token.register_callback([&] { ++callback_hits; });

    cts.cancel();
    cts.cancel(); // Second call must be a no-op

    REQUIRE(token.is_cancellation_requested());
    REQUIRE(callback_hits.load() == 1);
    REQUIRE_THROWS_AS(token.throw_if_cancellation_requested(), OperationCanceledException);

    // Registering callback after cancellation must execute immediately
    token.register_callback([&] { ++callback_hits; });
    REQUIRE(callback_hits.load() == 2);
}

TEST_CASE("async_sleep aborts immediately if token is already canceled or via cancel_after", "[cancellation][sleep]") {
    // 1. Pre-canceled token (hits await_ready() -> throws immediately)
    CancellationTokenSource pre_cts;
    pre_cts.cancel();

    auto pre_canceled_task = [](CancellationToken t) -> Task<void> {
        co_await async_sleep(1000ms, t);
    };
    REQUIRE_THROWS_AS(pre_canceled_task(pre_cts.token()).wait(), OperationCanceledException);

    // 2. Canceled mid-flight via cancel_after
    CancellationTokenSource timed_cts;
    timed_cts.cancel_after(30ms);

    auto mid_flight_task = [](CancellationToken t) -> Task<void> {
        co_await async_sleep(1000ms, t);
    };
    REQUIRE_THROWS_AS(mid_flight_task(timed_cts.token()).wait(), OperationCanceledException);
}

TEST_CASE("Cooperative cancellation with recording mock verification", "[cancellation][mock]") {
    RecordingWorker mock;

    CancellationTokenSource cts;

    auto pipeline = [&](CancellationToken token) -> Task<int> {
        int val = mock.step(10);
        try {
            co_await async_sleep(500ms, token);
            co_return mock.step(val);
        } catch (const OperationCanceledException&) {
            mock.on_cancel();
            throw;
        }
    };

    auto task = pipeline(cts.token());
    // Cancelling right away is fine: either the sleep is not armed yet (await_ready throws) or the
    // callback resumes it; both paths end in OperationCanceledException.
    cts.cancel();

    REQUIRE_THROWS_AS(task.result(), OperationCanceledException);
    std::lock_guard lock(mock.mtx);
    REQUIRE(mock.step_args == std::vector<int>{10});  // step(11) never happens
    REQUIRE(mock.cancel_calls == 1);
}

TEST_CASE("async_sleep with active token completes normally when not canceled", "[cancellation][sleep]") {
    CancellationTokenSource cts;

    auto normal_sleep = [](CancellationToken t) -> Task<int> {
        co_await async_sleep(0ms, t);   // Hits duration.count() <= 0
        co_await async_sleep(20ms, t);  // Completes timer without cancellation
        co_return 42;
    };

    REQUIRE(normal_sleep(cts.token()).result() == 42);
}

// --- Task 15: awaiter lifetime / reuse ---

namespace {

Task<int> await_same_sleep_twice(CancellationToken token) {
    auto sleep = async_sleep(10ms, token);
    co_await sleep;
    co_await sleep;
    co_return 1;
}

Task<int> sleep_and_swallow_cancel(CancellationToken token) {
    try {
        co_await async_sleep(1000ms, token);
    } catch (const OperationCanceledException&) {
    }
    co_return 1;
}

} // namespace

TEST_CASE("The same sleep awaiter can be awaited twice", "[cancellation][sleep]") {
    CancellationTokenSource cts;
    auto task = await_same_sleep_twice(cts.token());
    for (int i = 0; i < 400 && !task.await_ready(); ++i) std::this_thread::sleep_for(5ms);
    REQUIRE(task.await_ready());
    REQUIRE(task.result() == 1);
}

TEST_CASE("Concurrent cancel while suspending does not touch a destroyed awaiter", "[cancellation][race]") {
    for (int i = 0; i < 500; ++i) {
        CancellationTokenSource cts;
        auto task = sleep_and_swallow_cancel(cts.token());
        cts.cancel();
        REQUIRE(task.result() == 1);
    }
}

// --- Task 20: callback exception isolation ---

TEST_CASE("All cancellation callbacks run even if one throws", "[cancellation][exceptions]") {
    CancellationTokenSource cts;
    auto token = cts.token();
    std::atomic<int> hits{0};
    token.register_callback([] { throw std::runtime_error("boom"); });
    token.register_callback([&] { ++hits; });

    REQUIRE_THROWS_AS(cts.cancel(), std::runtime_error);
    REQUIRE(hits.load() == 1);
    REQUIRE(token.is_cancellation_requested());
}

// --- Task 21: moved-from source ---

TEST_CASE("Moved-from CancellationTokenSource is inert", "[cancellation][move]") {
    CancellationTokenSource a;
    auto token_a = a.token();
    CancellationTokenSource b(std::move(a));

    REQUIRE_NOTHROW(a.cancel());          // NOLINT(bugprone-use-after-move)
    REQUIRE_NOTHROW(a.cancel_after(1ms)); // NOLINT(bugprone-use-after-move)
    REQUIRE_FALSE(a.token().is_cancellation_requested()); // NOLINT(bugprone-use-after-move)
    REQUIRE_FALSE(token_a.is_cancellation_requested());

    b.cancel();
    REQUIRE(token_a.is_cancellation_requested());
}

// --- Task 23: registration ids ---

TEST_CASE("Callback registration ids and unregistering", "[cancellation][register]") {
    CancellationTokenSource cts;
    auto token = cts.token();
    std::atomic<int> a_hits{0};
    std::atomic<int> b_hits{0};

    auto id_a = token.register_callback([&] { ++a_hits; });
    auto id_b = token.register_callback([&] { ++b_hits; });
    REQUIRE(id_a != 0);
    REQUIRE(id_b != 0);
    REQUIRE(id_a != id_b);
    REQUIRE(token.registered_callback_count() == 2);

    REQUIRE(token.unregister_callback(id_a));
    REQUIRE_FALSE(token.unregister_callback(id_a));
    REQUIRE(token.registered_callback_count() == 1);

    cts.cancel();
    REQUIRE(a_hits.load() == 0);
    REQUIRE(b_hits.load() == 1);

    int late_hits = 0;
    REQUIRE(token.register_callback([&] { ++late_hits; }) == 0);
    REQUIRE(late_hits == 1);
}

TEST_CASE("Token without state ignores callback registration", "[cancellation][register]") {
    auto none = CancellationToken::none();
    REQUIRE(none.register_callback([] {}) == 0);
    REQUIRE(none.registered_callback_count() == 0);
    REQUIRE_FALSE(none.unregister_callback(1));
}

TEST_CASE("A callback may unregister another one during cancel without deadlock", "[cancellation][register]") {
    CancellationTokenSource cts;
    auto token = cts.token();
    std::atomic<std::uint64_t> victim{0};
    token.register_callback([&] { (void)token.unregister_callback(victim.load()); });
    victim = token.register_callback([] {});
    REQUIRE_NOTHROW(cts.cancel());
}

// --- Task 24: sleeping repeatedly on one token does not leak callbacks ---

namespace {

Task<void> sleep_many_times(CancellationToken token, int n) {
    for (int i = 0; i < n; ++i) {
        co_await async_sleep(1ms, token);
    }
}

} // namespace

TEST_CASE("Sleeping repeatedly on one token does not accumulate callbacks", "[cancellation][leak]") {
    CancellationTokenSource cts;
    auto token = cts.token();
    sleep_many_times(token, 200).wait();
    REQUIRE(token.registered_callback_count() == 0);
}

// --- Task 36: cancelled sleep resumes on the origin pool ---

namespace {

Task<ThreadPool*> cancelled_sleep_pool(CancellationToken token) {
    try {
        co_await async_sleep(5000ms, token);
    } catch (const OperationCanceledException&) {
    }
    co_return detail::current_pool;
}

struct PoolScope {
    ThreadPool* previous;
    explicit PoolScope(ThreadPool* p) : previous(detail::current_pool) { detail::current_pool = p; }
    ~PoolScope() { detail::current_pool = previous; }
};

} // namespace

TEST_CASE("Cancelled sleep resumes on the original pool", "[cancellation][affinity]") {
    ThreadPool pool(1);
    CancellationTokenSource cts;
    Task<ThreadPool*> task;
    {
        PoolScope scope(&pool);
        task = cancelled_sleep_pool(cts.token());
    }
    std::this_thread::sleep_for(30ms); // let the coroutine arm its sleep on `pool`
    cts.cancel();                      // fired from the main (non-pool) thread
    REQUIRE(task.result() == &pool);
}

// --- Task 37: cancel_after edge cases ---

TEST_CASE("cancel_after with zero or negative delay cancels synchronously", "[cancellation][timer]") {
    CancellationTokenSource zero;
    zero.cancel_after(0ms);
    REQUIRE(zero.token().is_cancellation_requested());

    CancellationTokenSource negative;
    negative.cancel_after(-5ms);
    REQUIRE(negative.token().is_cancellation_requested());
}

TEST_CASE("Manual cancel racing the cancel_after timer runs callbacks once", "[cancellation][timer]") {
    CancellationTokenSource cts;
    auto token = cts.token();
    std::atomic<int> hits{0};
    token.register_callback([&] { ++hits; });

    cts.cancel_after(20ms);
    std::this_thread::sleep_for(5ms);
    cts.cancel();
    std::this_thread::sleep_for(60ms); // the timer fires later and must be a no-op
    REQUIRE(hits.load() == 1);
}

TEST_CASE("cancel_after still fires after the source is destroyed", "[cancellation][timer]") {
    CancellationToken token;
    {
        CancellationTokenSource cts;
        token = cts.token();
        cts.cancel_after(20ms);
    }
    bool fired = false;
    for (int i = 0; i < 400 && !(fired = token.is_cancellation_requested()); ++i) std::this_thread::sleep_for(5ms);
    REQUIRE(fired);
}

TEST_CASE("Two cancel_after calls: the earlier deadline wins", "[cancellation][timer]") {
    CancellationTokenSource cts;
    cts.cancel_after(2000ms);
    cts.cancel_after(10ms);
    bool fired = false;
    for (int i = 0; i < 400 && !(fired = cts.token().is_cancellation_requested()); ++i) std::this_thread::sleep_for(5ms);
    REQUIRE(fired);
}

TEST_CASE("CancellationTokenSource(delay) cancels itself after the timeout", "[cancellation][timeout]") {
    CancellationTokenSource cts(30ms);
    REQUIRE_FALSE(cts.token().is_cancellation_requested());
    bool fired = false;
    for (int i = 0; i < 400 && !(fired = cts.token().is_cancellation_requested()); ++i) std::this_thread::sleep_for(5ms);
    REQUIRE(fired);
}

TEST_CASE("timeout pattern: OperationCanceledException is converted to TimeoutException", "[cancellation][timeout]") {
    auto long_running = [](CancellationToken token) -> Task<std::string> {
        co_await async_sleep(5000ms, token);
        co_return std::string("never");
    };
    auto with_timeout = [&](std::chrono::milliseconds limit) -> Task<std::string> {
        CancellationTokenSource cts(limit);
        try {
            co_return co_await long_running(cts.token());
        } catch (const OperationCanceledException&) {
            throw TimeoutException("The operation timed out.");
        }
    };

    const auto start = std::chrono::steady_clock::now();
    REQUIRE_THROWS_AS(with_timeout(50ms).result(), TimeoutException);
    REQUIRE(std::chrono::steady_clock::now() - start < 3000ms);
}

TEST_CASE("timeout does not fire when the operation finishes in time", "[cancellation][timeout]") {
    CancellationTokenSource cts(2000ms);
    auto quick = [](CancellationToken token) -> Task<int> {
        co_await async_sleep(10ms, token);
        co_return 7;
    };
    REQUIRE(quick(cts.token()).result() == 7);
    REQUIRE_FALSE(cts.token().is_cancellation_requested());
}

TEST_CASE("TimeoutException is a std::runtime_error with a default message", "[cancellation][timeout]") {
    TimeoutException ex;
    REQUIRE(std::string(ex.what()) == "The operation timed out.");
    REQUIRE(std::string(TimeoutException("custom").what()) == "custom");
    static_assert(std::is_base_of_v<std::runtime_error, TimeoutException>);
}
