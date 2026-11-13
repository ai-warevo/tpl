#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/threading/tasks/task.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading::tasks;

TEST_CASE("Task default construction and already-completed fast paths", "[task]") {
    Task<int> empty_task;
    REQUIRE(empty_task.await_ready());

    auto fast_coro = []() -> Task<int> {
        co_return 77;
    };

    auto t = fast_coro();
    std::this_thread::sleep_for(30ms); // Let worker complete task before awaiting
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
    TaskState<int> ts;
    REQUIRE_FALSE(ts.is_ready());

    // std::noop_coroutine() is a valid, safe no-op coroutine handle in C++20
    REQUIRE(ts.try_attach_continuation(std::noop_coroutine()));
    REQUIRE(ts.try_attach_continuation(std::noop_coroutine()));

    ts.complete();
    REQUIRE(ts.is_ready());

    // Attaching after completion must return false
    REQUIRE_FALSE(ts.try_attach_continuation(std::noop_coroutine()));
}