#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/cancellation.hpp>
#include <aw/tpl/when_any.hpp>

#include <chrono>
#include <thread>
#include <memory>
#include <string>
#include <type_traits>
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

    Task<int> winner = when_any(racers).result();  // Task<Task<int>>: the winning task itself
    REQUIRE(winner == racers[1]);                   // identity, like C# reference equality
    REQUIRE(winner.result() == 200);

    // 2. Fast path: task already finished before when_any suspends
    auto instant = []() -> Task<int> { co_return 555; }();
    REQUIRE(wait_until([&] { return instant.await_ready(); }));

    std::vector<Task<int>> pre_completed;
    pre_completed.push_back(instant);
    pre_completed.push_back([]() -> Task<int> { co_await async_sleep(100ms); co_return 666; }());

    Task<int> fast = when_any(pre_completed).result();
    REQUIRE(fast == pre_completed[0]);
    REQUIRE(fast.result() == 555);
}

TEST_CASE("when_any on vector reports a failed task as the winner", "[when_any][vector][exceptions]") {
    std::vector<Task<int>> tasks;
    tasks.push_back([]() -> Task<int> { co_await async_sleep(200ms); co_return 1; }());
    tasks.push_back([]() -> Task<int> {
        co_await async_sleep(10ms);
        throw std::runtime_error("boom");
        co_return 2;
    }());
    // The outer task succeeds; the failure surfaces when the winner itself is awaited.
    Task<int> winner = when_any(tasks).result();
    REQUIRE(winner == tasks[1]);
    REQUIRE_THROWS_AS(winner.result(), std::runtime_error);
}

TEST_CASE("when_any variadic with the same type returns Task<Task<T>>", "[when_any][variadic]") {
    static_assert(std::is_same_v<decltype(when_any(std::declval<Task<int>>(), std::declval<Task<int>>())), Task<Task<int>>>);

    auto slow = []() -> Task<int> { co_await async_sleep(200ms); co_return 1; }();
    auto fast = []() -> Task<int> { co_await async_sleep(15ms); co_return 2; }();

    Task<int> winner = when_any(slow, fast).result();
    REQUIRE(winner == fast);
    REQUIRE(winner != slow);
    REQUIRE(winner.result() == 2);
    REQUIRE(slow.result() == 1);   // the loser keeps running and is still readable
}

TEST_CASE("when_any variadic with mixed types returns Task<Task<void>>", "[when_any][variadic][mixed]") {
    static_assert(std::is_same_v<decltype(when_any(std::declval<Task<int>>(), std::declval<Task<void>>())), Task<Task<void>>>);

    auto number = []() -> Task<int> { co_await async_sleep(200ms); co_return 1; }();
    auto nothing = []() -> Task<void> { co_await async_sleep(200ms); }();
    auto text = []() -> Task<std::string> { co_await async_sleep(15ms); co_return std::string("hi"); }();

    Task<void> winner = when_any(number, nothing, text).result();
    REQUIRE(winner == text);          // Task<void> vs Task<std::string>: same underlying task
    REQUIRE(winner != number);
    REQUIRE(winner != nothing);
    REQUIRE_NOTHROW(winner.wait());
    REQUIRE(text.result() == "hi");   // the typed handle still yields the value
}

TEST_CASE("when_any variadic reports a failed task as the winner", "[when_any][variadic][exceptions]") {
    auto slow = []() -> Task<int> { co_await async_sleep(200ms); co_return 1; }();
    auto slow_void = []() -> Task<void> { co_await async_sleep(200ms); }();
    auto failing = []() -> Task<int> {
        co_await async_sleep(15ms);
        throw std::runtime_error("fast fail");
        co_return 3;
    }();

    Task<void> winner = when_any(slow, slow_void, failing).result();
    REQUIRE(winner == failing);
    REQUIRE_THROWS_AS(winner.wait(), std::runtime_error);
    REQUIRE_THROWS_AS(failing.result(), std::runtime_error);
}

TEST_CASE("Task<void> view shares state with its source task", "[when_any][task]") {
    auto t = []() -> Task<std::unique_ptr<int>> { co_return std::make_unique<int>(5); }();
    Task<void> view = t;
    REQUIRE(view == t);
    REQUIRE(t == view);
    view.wait();                       // waiting through the view must not consume the move-only value
    REQUIRE(*t.result() == 5);
    REQUIRE(Task<int>{} == Task<void>{});
}

TEST_CASE("when_any with a single task returns it", "[when_any][variadic]") {
    auto only = []() -> Task<int> { co_await async_sleep(10ms); co_return 9; }();
    Task<int> winner = when_any(only).result();
    REQUIRE(winner == only);
    REQUIRE(winner.result() == 9);
}

TEST_CASE("when_any + cancel the rest (C# WhenAny pattern)", "[when_any][cancellation]") {
    CancellationTokenSource cts;
    auto fetch = [](std::chrono::milliseconds d, int v, CancellationToken token) -> Task<int> {
        co_await async_sleep(d, token);
        co_return v;
    };
    Task<int> a = fetch(5000ms, 1, cts.token());
    Task<int> b = fetch(20ms, 2, cts.token());

    Task<int> completed = when_any(a, b).result();
    cts.cancel();                                  // cancel the remaining task to save resources
    REQUIRE(completed.result() == 2);
    REQUIRE_THROWS_AS(a.result(), OperationCanceledException);
}

TEST_CASE("when_any works with vector of Task<void>", "[when_any][void]") {
    std::vector<Task<void>> tasks;
    tasks.push_back([]() -> Task<void> { co_await async_sleep(150ms); co_return; }());
    tasks.push_back([]() -> Task<void> { co_await async_sleep(15ms); co_return; }());

    Task<void> winner = when_any(tasks).result();
    REQUIRE(winner == tasks[1]);
    REQUIRE_NOTHROW(winner.wait());
}

TEST_CASE("when_any handles winning Task<void> and concurrent completion races", "[when_any][void]") {
    // 1. Task<void> wins the race against a Task<int>
    auto slow_int = []() -> Task<int> { co_await async_sleep(150ms); co_return 1; }();
    auto fast_void = []() -> Task<void> { co_await async_sleep(10ms); co_return; }();
    REQUIRE(when_any(slow_int, fast_void).result() == fast_void);

    // 2. Vector of Task<void>
    std::vector<Task<void>> void_racers;
    void_racers.push_back([]() -> Task<void> { co_await async_sleep(150ms); co_return; }());
    void_racers.push_back([]() -> Task<void> { co_await async_sleep(10ms); co_return; }());
    REQUIRE(when_any(void_racers).result() == void_racers[1]);

    // 3. Stress rapid completion race to hit await_suspend fast-return path
    for (int i = 0; i < 50; ++i) {
        auto one = []() -> Task<int> { co_return 1; }();
        auto two = []() -> Task<int> { co_return 2; }();
        auto winner = when_any(one, two).result();
        REQUIRE((winner == one || winner == two));
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
        auto slow = []() -> Task<int> { co_await async_sleep(500ms); co_return 100; }();
        auto instant = []() -> Task<int> { co_return 200; }();
        REQUIRE(when_any(slow, instant).result() == instant);
    }
}

TEST_CASE("when_any does not consume a move-only result of the winner", "[when_any][move]") {
    std::vector<Task<std::unique_ptr<int>>> tasks;
    tasks.push_back([]() -> Task<std::unique_ptr<int>> {
        co_await async_sleep(10ms);
        co_return std::make_unique<int>(42);
    }());
    Task<std::unique_ptr<int>> winner = when_any(tasks).result();
    REQUIRE(winner == tasks[0]);
    auto p = winner.result();
    REQUIRE(p != nullptr);
    REQUIRE(*p == 42);
}

TEST_CASE("WhenAnyAwaiter::await_suspend is not noexcept", "[when_any][noexcept]") {
    static_assert(!noexcept(std::declval<detail::WhenAnyAwaiter&>().await_suspend(std::noop_coroutine())));
    SUCCEED();
}
