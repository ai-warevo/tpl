#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/when_any.hpp>

#include <chrono>
#include <thread>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace std::chrono_literals;
using namespace aw::tpl;
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
    REQUIRE(wait_until([&] { return instant.await_ready(); }));

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

TEST_CASE("when_any works with vector of Task<void>", "[when_any][void]") {
    std::vector<Task<void>> tasks;
    tasks.push_back([]() -> Task<void> { co_await async_sleep(150ms); co_return; }());
    tasks.push_back([]() -> Task<void> { co_await async_sleep(15ms); co_return; }());

    Task<void> winner = when_any(std::move(tasks)).result();
    REQUIRE_NOTHROW(winner.wait());
}

TEST_CASE("when_any handles winning Task<void> and concurrent completion races", "[when_any][void]") {
    // 1. Task<void> wins the race (covers is_void_v<T> branch in spawn_when_any_watcher)
    size_t winner_idx = when_any(
        []() -> Task<int> { co_await async_sleep(150ms); co_return 1; }(),
        []() -> Task<void> { co_await async_sleep(10ms); co_return; }()
    ).result();
    REQUIRE(winner_idx == 1);

    // 2. Vector of Task<void>
    std::vector<Task<void>> void_racers;
    void_racers.push_back([]() -> Task<void> { co_await async_sleep(150ms); co_return; }());
    void_racers.push_back([]() -> Task<void> { co_await async_sleep(10ms); co_return; }());
    REQUIRE_NOTHROW(when_any(std::move(void_racers)).result().wait());

    // 3. Stress rapid completion race to hit await_suspend fast-return path
    for (int i = 0; i < 50; ++i) {
        auto idx = when_any(
            []() -> Task<int> { co_return 1; }(),
            []() -> Task<int> { co_return 2; }()
        ).result();
        REQUIRE((idx == 0 || idx == 1));
    }
}

TEST_CASE("WhenAnyAwaiter await_suspend returns caller handle when already completed", "[when_any][race]") {
    auto state = std::make_shared<detail::WhenAnyState>();
    state->completed.store(true, std::memory_order_release);

    detail::WhenAnyAwaiter awaiter{state};
    auto caller = std::noop_coroutine();
    REQUIRE(awaiter.await_suspend(caller) == caller);
}

TEST_CASE("when_any reports the correct index under immediate completion races", "[when_any][race]") {
    for (int i = 0; i < 2000; ++i) {
        size_t idx = when_any(
            []() -> Task<int> { co_await async_sleep(500ms); co_return 100; }(),
            []() -> Task<int> { co_return 200; }()
        ).result();
        REQUIRE(idx == 1);
    }
}

TEST_CASE("when_any does not consume a move-only result of the winner", "[when_any][move]") {
    std::vector<Task<std::unique_ptr<int>>> tasks;
    tasks.push_back([]() -> Task<std::unique_ptr<int>> {
        co_await async_sleep(10ms);
        co_return std::make_unique<int>(42);
    }());
    Task<std::unique_ptr<int>> winner = when_any(std::move(tasks)).result();
    auto p = winner.result();
    REQUIRE(p != nullptr);
    REQUIRE(*p == 42);
}

TEST_CASE("WhenAnyAwaiter::await_suspend is not noexcept", "[when_any][noexcept]") {
    static_assert(!noexcept(std::declval<detail::WhenAnyAwaiter&>().await_suspend(std::noop_coroutine())));
    SUCCEED();
}
