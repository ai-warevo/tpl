#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/threading/tasks/when_all.hpp>
#include <aw/tpl/threading/tasks/async_sleep.hpp>

#include <atomic>
#include <memory>
#include <chrono>
#include <thread>
#include <tuple>
#include <stdexcept>
#include <string>
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

TEST_CASE("when_all handles heterogeneous tasks including void", "[when_all]") {
    // The coroutine runs on a pool worker, so all assertions are made on the main thread.
    auto runner = []() -> Task<std::tuple<int, std::string, std::monostate>> {
        co_return co_await when_all(
            []() -> Task<int> {
                co_await async_sleep(20ms);
                co_return 12;
            }(),
            []() -> Task<std::string> {
                co_await async_sleep(10ms);
                co_return "tpl";
            }(),
            []() -> Task<void> {
                co_await async_sleep(15ms);
                co_return;
            }());
    };

    auto [num, text, unit] = runner().result();
    REQUIRE(num == 12);
    REQUIRE(text == "tpl");
    REQUIRE(std::is_same_v<decltype(unit), std::monostate>);
}

TEST_CASE("when_all handles vectors of Task<T> and Task<void>", "[when_all][vector]") {
    // 1. Vector of Task<int>
    std::vector<Task<int>> int_tasks;
    for (int i = 1; i <= 4; ++i) {
        int_tasks.push_back([](int x) -> Task<int> {
            co_await async_sleep(15ms);
            co_return x * 10;
        }(i));
    }
    auto results = when_all(std::move(int_tasks)).result();
    REQUIRE(results == std::vector<int>{10, 20, 30, 40});

    // 2. Vector of Task<void> (pass counter by parameter to avoid dangling lambda capture)
    std::atomic<int> counter{0};
    std::vector<Task<void>> void_tasks;
    for (int i = 0; i < 3; ++i) {
        void_tasks.push_back([](std::atomic<int>& cnt) -> Task<void> {
            co_await async_sleep(10ms);
            ++cnt;
            co_return;
        }(counter));
    }
    when_all(std::move(void_tasks)).wait();
    REQUIRE(counter.load() == 3);

    // 3. Empty vectors
    std::vector<Task<int>> empty_ints;
    REQUIRE(when_all(std::move(empty_ints)).result().empty());

    std::vector<Task<void>> empty_voids;
    REQUIRE_NOTHROW(when_all(std::move(empty_voids)).wait());
}

TEST_CASE("when_all propagates exceptions from failed child tasks", "[when_all][exceptions]") {
    std::vector<Task<int>> tasks;
    tasks.push_back([]() -> Task<int> {
        co_await async_sleep(10ms);
        co_return 1;
    }());
    tasks.push_back([]() -> Task<int> {
        co_await async_sleep(20ms);
        throw std::runtime_error("child failed");
        co_return 2;
    }());

    REQUIRE_THROWS_AS(when_all(std::move(tasks)).result(), std::runtime_error);
}

TEST_CASE("when_all fast-paths already completed tasks", "[when_all][fast]") {
    auto t1 = []() -> Task<int> { co_return 10; }();
    auto t2 = []() -> Task<void> { co_return; }();
    auto t3 = []() -> Task<void> { co_return; }();

    REQUIRE(wait_until([&] { return t1.await_ready() && t2.await_ready() && t3.await_ready(); }));

    auto [res, unit] = when_all(t1, t2).result();
    REQUIRE(res == 10);

    std::vector<Task<void>> done_voids{t2, t3};
    REQUIRE_NOTHROW(when_all(std::move(done_voids)).wait());
}

// --- Tasks 16-17: wait for every child before failing ---

namespace {

Task<int> fail_immediately() {
    throw std::runtime_error("fail");
    co_return 0;
}

Task<int> slow_bump(std::atomic<int>& counter) {
    co_await async_sleep(60ms);
    ++counter;
    co_return 1;
}

Task<void> fail_immediately_void() {
    throw std::runtime_error("fail");
    co_return;
}

Task<void> slow_bump_void(std::atomic<int>& counter) {
    co_await async_sleep(60ms);
    ++counter;
}

} // namespace

TEST_CASE("when_all waits for every child before failing", "[when_all][exceptions]") {
    std::atomic<int> counter{0};
    std::vector<Task<int>> tasks;
    tasks.push_back(fail_immediately());
    tasks.push_back(slow_bump(counter));
    REQUIRE_THROWS_AS(when_all(std::move(tasks)).result(), std::runtime_error);
    REQUIRE(counter.load() == 1);

    std::atomic<int> void_counter{0};
    std::vector<Task<void>> void_tasks;
    void_tasks.push_back(fail_immediately_void());
    void_tasks.push_back(slow_bump_void(void_counter));
    REQUIRE_THROWS_AS(when_all(std::move(void_tasks)).wait(), std::runtime_error);
    REQUIRE(void_counter.load() == 1);
}

TEST_CASE("variadic when_all waits for all children on failure", "[when_all][exceptions]") {
    std::atomic<int> counter{0};
    REQUIRE_THROWS_AS(when_all(fail_immediately(), slow_bump(counter)).result(), std::runtime_error);
    REQUIRE(counter.load() == 1);

    // The failing child is last: earlier children must still be awaited.
    std::atomic<int> counter2{0};
    REQUIRE_THROWS_AS(when_all(slow_bump(counter2), fail_immediately()).result(), std::runtime_error);
    REQUIRE(counter2.load() == 1);
}

TEST_CASE("variadic when_all supports move-only and container results", "[when_all][move]") {
    auto [p, v] = when_all([]() -> Task<std::unique_ptr<int>> { co_return std::make_unique<int>(3); }(),
                           []() -> Task<std::vector<std::unique_ptr<int>>> {
                               std::vector<std::unique_ptr<int>> out;
                               out.push_back(std::make_unique<int>(4));
                               co_return out;
                           }())
                      .result();
    REQUIRE(*p == 3);
    REQUIRE(*v.at(0) == 4);
}
