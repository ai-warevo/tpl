#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/task.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>

using namespace std::chrono_literals;
using namespace aw::tpl;

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
