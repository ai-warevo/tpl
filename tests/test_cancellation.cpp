#include <catch2/catch_test_macros.hpp>
#include <fakeit.hpp>
#include <aw/tpl/tpl.hpp>

#include <atomic>
#include <chrono>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading::tasks;
using namespace fakeit;

namespace {

struct IWorker {
    virtual ~IWorker() = default;
    virtual int step(int x) = 0;
    virtual void on_cancel() = 0;
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

TEST_CASE("Cooperative cancellation with FakeIt mock verification", "[cancellation][mock]") {
    Mock<IWorker> mock;
    When(Method(mock, step)).AlwaysDo([](int x) { return x + 1; });
    Fake(Method(mock, on_cancel));

    CancellationTokenSource cts;

    auto pipeline = [&](CancellationToken token) -> Task<int> {
        int val = mock.get().step(10);
        try {
            co_await async_sleep(500ms, token);
            co_return mock.get().step(val);
        } catch (const OperationCanceledException&) {
            mock.get().on_cancel();
            throw;
        }
    };

    auto task = pipeline(cts.token());
    // Cancelling right away is fine: either the sleep is not armed yet (await_ready throws) or the
    // callback resumes it; both paths end in OperationCanceledException.
    cts.cancel();

    REQUIRE_THROWS_AS(task.result(), OperationCanceledException);
    Verify(Method(mock, step).Using(10)).Once();
    Verify(Method(mock, step).Using(11)).Never();
    Verify(Method(mock, on_cancel)).Once();
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
