#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/threading/tasks/task.hpp>
#include <aw/tpl/threading/tasks/when_all.hpp>
#include <aw/tpl/threading/tasks/async_sleep.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading;
using namespace aw::tpl::threading::tasks;

namespace {

// Polls `pred` every millisecond until it holds or `timeout` elapses.
template <typename Pred>
bool wait_until(Pred pred, std::chrono::milliseconds timeout = 5000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!pred()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

} // namespace

TEST_CASE("Task default construction and already-completed fast paths", "[task]") {
    Task<int> empty_task;
    REQUIRE(empty_task.await_ready());

    auto fast_coro = []() -> Task<int> {
        co_return 77;
    };

    auto t = fast_coro();
    REQUIRE(wait_until([&] { return t.await_ready(); })); // let the worker complete the task
    REQUIRE(t.await_ready());
    REQUIRE(t.result() == 77);

    auto consumer = [](Task<int> already_done) -> Task<int> {
        co_return co_await already_done;
    };
    REQUIRE(consumer(t).result() == 77);
}

TEST_CASE("Task supports move-only return types like std::unique_ptr", "[task][move]") {
    auto make_ptr = []() -> Task<std::unique_ptr<int>> {
        co_await async_sleep(15ms);
        co_return std::make_unique<int>(999);
    };

    auto ptr = make_ptr().result();
    REQUIRE(ptr != nullptr);
    REQUIRE(*ptr == 999);
}

TEST_CASE("Task<void> and Task<T> propagate exceptions properly", "[task][exceptions]") {
    auto throw_int = []() -> Task<int> {
        co_await async_sleep(10ms);
        throw std::logic_error("int error");
        co_return 0;
    };

    auto throw_void = []() -> Task<void> {
        co_await async_sleep(10ms);
        throw std::runtime_error("void error");
    };

    REQUIRE_THROWS_AS(throw_int().result(), std::logic_error);
    REQUIRE_THROWS_AS(throw_void().wait(), std::runtime_error);
}

TEST_CASE("Task::run propagates exceptions from throwing lambdas", "[task][run][exceptions]") {
    auto task = Task<>::run([]() -> int {
        throw std::runtime_error("lambda boom");
    });

    REQUIRE_THROWS_AS(task.result(), std::runtime_error);
}

TEST_CASE("Task handles exceptions in Task::run and rapid co_await completion races", "[task][race]") {
    auto throwing_run = Task<>::run([]() -> int {
        throw std::runtime_error("run failure");
    });
    REQUIRE_THROWS_AS(throwing_run.result(), std::runtime_error);

    // Stress instant completion race between await_ready() and try_attach_continuation()
    auto race_coro = []() -> Task<int> {
        int sum = 0;
        for (int i = 0; i < 100; ++i) {
            auto instant = []() -> Task<int> { co_return 1; }();
            sum += co_await instant;
        }
        co_return sum;
    };
    REQUIRE(race_coro().result() == 100);
}

TEST_CASE("Task await_suspend returns caller handle if completed concurrently after await_ready", "[task][race]") {
    auto t = []() -> Task<int> { co_return 1; }();
    REQUIRE(t.result() == 1);

    auto caller = std::noop_coroutine();
    REQUIRE(t.await_suspend(caller) == caller);
}

TEST_CASE("Multiple coroutines can await the same Task", "[task][continuation]") {
    auto slow_task = []() -> Task<int> {
        co_await async_sleep(80ms);
        co_return 7;
    }();

    auto consumer = [](Task<int> already_done) -> Task<int> {
        co_return co_await already_done;
    };

    // Create copies of the task to simulate multiple consumers
    auto t1 = slow_task;
    auto t2 = slow_task;
    auto t3 = slow_task;

    auto c1 = consumer(t1);
    auto c2 = consumer(t2);
    auto c3 = consumer(t3);

    // Wait on all consumers and check results
    REQUIRE(c1.result() == 7);
    REQUIRE(c2.result() == 7);
    REQUIRE(c3.result() == 7);
}

TEST_CASE("TaskState keeps every attached continuation", "[task][continuation]") {
    threading::tasks::detail::TaskState<int> ts;
    REQUIRE_FALSE(ts.is_ready());

    // std::noop_coroutine() is a valid, safe no-op coroutine handle in C++20
    REQUIRE(ts.try_attach_continuation(std::noop_coroutine()));
    REQUIRE(ts.try_attach_continuation(std::noop_coroutine()));

    ts.complete();
    REQUIRE(ts.is_ready());

    // Attaching after completion must return false
    REQUIRE_FALSE(ts.try_attach_continuation(std::noop_coroutine()));
}

// --- New tests for Task 02 ---

TEST_CASE("TaskState::finish returns noop when nobody waits", "[task][state]") {
    threading::tasks::detail::TaskState<void> ts;
    REQUIRE_FALSE(ts.is_ready());

    // Check that the finish returns noop when no continuations are attached
    auto handle = ts.finish();
    REQUIRE(handle == std::noop_coroutine());
    REQUIRE(ts.is_ready());
    
    // Check that we can't attach after completion
    REQUIRE_FALSE(ts.try_attach_continuation(std::noop_coroutine()));
}

TEST_CASE("TaskState::finish hands back the first continuation", "[task][state]") {
    threading::tasks::detail::TaskState<int> ts;
    std::coroutine_handle<> first_handle = std::noop_coroutine();
    REQUIRE(ts.try_attach_continuation(first_handle));
    
    // Attach a second handle
    std::coroutine_handle<> second_handle = std::noop_coroutine();
    REQUIRE(ts.try_attach_continuation(second_handle));
    
    // Capture the returned handle
    auto returned_handle = ts.finish();

    // Check that the first handle was returned by finish()
    REQUIRE(returned_handle == first_handle);
}

// --- Task 03: completion happens in final_suspend ---

namespace {

struct SlowDtorGuard {
    std::atomic<bool>* flag;
    ~SlowDtorGuard() {
        std::this_thread::sleep_for(50ms);
        flag->store(true);
    }
};

Task<void> run_with_slow_local(std::atomic<bool>& flag) {
    SlowDtorGuard guard{&flag};
    co_return;
}

Task<int> chain_depth(int n) {
    if (n == 0) co_return 0;
    co_return 1 + co_await chain_depth(n - 1);
}

} // namespace

TEST_CASE("Waiter is released only after coroutine locals are destroyed", "[task][final_suspend]") {
    std::atomic<bool> destroyed{false};
    auto t = run_with_slow_local(destroyed);
    t.wait();
    REQUIRE(destroyed.load());
}

TEST_CASE("Long chains of awaiting tasks do not overflow the stack", "[task][chain]") {
    REQUIRE(chain_depth(2000).result() == 2000);

#if defined(NDEBUG) && !defined(__SANITIZE_ADDRESS__) && !defined(__SANITIZE_THREAD__)
    // Symmetric transfer is only guaranteed by GCC with optimisation and without sanitizers.
    REQUIRE(chain_depth(200000).result() == 200000);
#endif
}

// --- Task 08: empty state ---

TEST_CASE("Empty and moved-from tasks throw logic_error instead of crashing", "[task][empty]") {
    REQUIRE_THROWS_AS(Task<int>{}.result(), std::logic_error);
    REQUIRE_THROWS_AS(Task<void>{}.wait(), std::logic_error);

    auto ready = []() -> Task<int> { co_return 1; }();
    REQUIRE(ready.result() == 1);
    Task<int> other = std::move(ready);
    REQUIRE_THROWS_AS(ready.result(), std::logic_error); // NOLINT(bugprone-use-after-move)
    REQUIRE(other.result() == 1);

    auto awaiter = [](Task<int> t) -> Task<bool> {
        try {
            (void)co_await t;
        } catch (const std::logic_error&) {
            co_return true;
        }
        co_return false;
    };
    REQUIRE(awaiter(Task<int>{}).result());
}

// --- Tasks 09-11: value storage ---

namespace {

struct NoDefault {
    explicit NoDefault(int x) : v(x) {}
    int v;
};

Task<std::pair<int, int>> make_pair_task() { co_return {3, 4}; }
Task<NoDefault> make_nodefault_task() { co_return NoDefault{5}; }
Task<std::string> make_string_task() { co_return "abc"; }

Task<std::vector<std::unique_ptr<int>>> make_ptr_vector() {
    std::vector<std::unique_ptr<int>> v;
    v.push_back(std::make_unique<int>(1));
    v.push_back(std::make_unique<int>(2));
    co_return v;
}

Task<std::unique_ptr<int>> make_ptr_task(int v) {
    co_await async_sleep(5ms);
    co_return std::make_unique<int>(v);
}

} // namespace

TEST_CASE("Task result type does not need to be default constructible", "[task][value]") {
    REQUIRE(make_nodefault_task().result().v == 5);
}

TEST_CASE("co_return accepts braced initializers and converting values", "[task][value]") {
    REQUIRE(make_pair_task().result() == std::pair<int, int>{3, 4});
    REQUIRE(make_string_task().result() == "abc");
}

TEST_CASE("Task of vector<unique_ptr> is movable out", "[task][move]") {
    auto v = make_ptr_vector().result();
    REQUIRE(v.size() == 2);
    REQUIRE(*v[0] == 1);
    REQUIRE(*v[1] == 2);
}

TEST_CASE("when_all over move-only tasks", "[when_all][move]") {
    std::vector<Task<std::unique_ptr<int>>> tasks;
    for (int i = 1; i <= 3; ++i) tasks.push_back(make_ptr_task(i * 10));
    auto results = when_all(std::move(tasks)).result();
    REQUIRE(results.size() == 3);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(results[static_cast<size_t>(i)] != nullptr);
        REQUIRE(*results[static_cast<size_t>(i)] == (i + 1) * 10);
    }
}

TEST_CASE("is_truly_copyable looks inside containers", "[task][trait]") {
    static_assert(!threading::tasks::detail::is_truly_copyable_v<std::vector<std::unique_ptr<int>>>);
    static_assert(threading::tasks::detail::is_truly_copyable_v<std::vector<int>>);
    static_assert(!threading::tasks::detail::is_truly_copyable_v<std::optional<std::unique_ptr<int>>>);
    static_assert(!threading::tasks::detail::is_truly_copyable_v<std::pair<int, std::unique_ptr<int>>>);
    SUCCEED();
}

// --- Tasks 12-13: one-shot extraction ---

TEST_CASE("TaskState::try_take is one-shot", "[task][state]") {
    threading::tasks::detail::TaskState<int> ts;
    REQUIRE(ts.try_take());
    REQUIRE_FALSE(ts.try_take());
}

TEST_CASE("Second result() of a move-only task throws TaskAlreadyMovedException", "[task][move]") {
    auto t = make_ptr_task(7);
    auto first = t.result();
    REQUIRE(first != nullptr);
    REQUIRE_THROWS_AS(t.result(), TaskAlreadyMovedException);
}

TEST_CASE("Concurrent extraction of a move-only result yields exactly one value", "[task][move][race]") {
    auto t = make_ptr_task(9);
    REQUIRE(wait_until([&] { return t.await_ready(); }));

    std::atomic<int> successes{0};
    std::atomic<int> already_moved{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([t, &successes, &already_moved]() {
            try {
                auto p = t.result();
                if (p) ++successes;
            } catch (const TaskAlreadyMovedException&) {
                ++already_moved;
            }
        });
    }
    for (auto& th : threads) th.join();
    REQUIRE(successes.load() == 1);
    REQUIRE(already_moved.load() == 7);
}

// --- Task 14: blocking waits on workers ---

TEST_CASE("Blocking wait on a pool worker throws instead of deadlocking", "[task][deadlock]") {
    auto outer = []() -> Task<int> {
        auto inner = []() -> Task<int> {
            co_await async_sleep(30ms);
            co_return 1;
        }();
        bool threw = false;
        try {
            (void)inner.result();
        } catch (const std::logic_error&) {
            threw = true;
        }
        (void)co_await inner; // still usable asynchronously
        co_return threw ? 1 : 0;
    }();
    REQUIRE(outer.result() == 1);
}

TEST_CASE("result() on an already-ready task works on a worker", "[task][deadlock]") {
    auto inner = []() -> Task<int> { co_return 5; }();
    REQUIRE(wait_until([&] { return inner.await_ready(); }));

    auto outer = [](Task<int> in) -> Task<int> { co_return in.result(); }(inner);
    REQUIRE(outer.result() == 5);
}

// --- Task 28: Task<>::run coverage ---

namespace {

struct MoveOnlyCallable {
    std::unique_ptr<int> value;
    int operator()() { return *value; }
};

} // namespace

TEST_CASE("Task<>::run covers void, by-value capture and move-only callables", "[task][run]") {
    // 1. void callable
    auto flag = std::make_shared<std::atomic<bool>>(false);
    Task<>::run([flag] { flag->store(true); }).wait();
    REQUIRE(flag->load());

    // 2. closure created in a scope that ends before the task is consumed
    Task<std::string> from_scope;
    {
        std::string text = "captured by value";
        from_scope = Task<>::run([text] { return text; });
    }
    REQUIRE(from_scope.result() == "captured by value");

    // 3. move-only callable
    REQUIRE(Task<>::run(MoveOnlyCallable{std::make_unique<int>(31)}).result() == 31);

    // 4. nested run awaited (not blocked on) from inside a coroutine
    auto outer = []() -> Task<int> {
        int inner = co_await Task<>::run([] { return 20; });
        co_return inner + 1;
    };
    REQUIRE(outer().result() == 21);
}

TEST_CASE("Task::await_suspend is not noexcept", "[task][noexcept]") {
    static_assert(!noexcept(std::declval<Task<int>&>().await_suspend(std::noop_coroutine())));
    SUCCEED();
}
