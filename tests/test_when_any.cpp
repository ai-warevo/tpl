#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/when_any.hpp>

#include <chrono>
#include <stdexcept>
#include <vector>

using namespace std::chrono_literals;
using namespace aw::tpl;

TEST_CASE("when_any throws on empty vector", "[when_any][exceptions]") {
    std::vector<Task<int>> empty;
    REQUIRE_THROWS_AS(when_any(std::move(empty)).result(), std::invalid_argument);
}

TEST_CASE("when_any on vector returns first completed task including fast path", "[when_any][vector]") {
    // 1. Race between slow and fast tasks
    std::vector<Task<int>> racers;
    racers.push_back([]() -> Task<int> { co_await async_sleep(200ms); co_return 100; }());
    racers.push_back([]() -> Task<int> { co_await async_sleep(20ms); co_return 200; }());

    Task<int> winner = when_any(std::move(racers)).result();
    REQUIRE(winner.result() == 200);

    // 2. Fast path: task already finished before when_any suspends
    auto instant = []() -> Task<int> { co_return 555; }();
    std::this_thread::sleep_for(25ms);

    std::vector<Task<int>> pre_completed;
    pre_completed.push_back(instant);
    pre_completed.push_back([]() -> Task<int> { co_await async_sleep(100ms); co_return 666; }());

    REQUIRE(when_any(std::move(pre_completed)).result().result() == 555);
}

TEST_CASE("when_any variadic returns exact winner index even on exception", "[when_any][variadic]") {
    size_t idx = when_any(
        []() -> Task<int> { co_await async_sleep(200ms); co_return 1; }(),
        []() -> Task<void> { co_await async_sleep(200ms); }(),
        []() -> Task<int> {
            co_await async_sleep(15ms);
            throw std::runtime_error("fast fail");
            co_return 3;
        }()
    ).result();

    REQUIRE(idx == 2);
}
