#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/when_all.hpp>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using namespace aw::tpl;

TEST_CASE("when_all handles heterogeneous tasks including void", "[when_all]") {
    auto runner = []() -> Task<void> {
        auto [num, text, unit] = co_await when_all(
            []() -> Task<int> { co_await async_sleep(20ms); co_return 12; }(),
            []() -> Task<std::string> { co_await async_sleep(10ms); co_return "tpl"; }(),
            []() -> Task<void> { co_await async_sleep(15ms); co_return; }()
        );

        REQUIRE(num == 12);
        REQUIRE(text == "tpl");
        REQUIRE(std::is_same_v<decltype(unit), std::monostate>);
        co_return;
    };

    runner().wait();
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
    tasks.push_back([]() -> Task<int> { co_await async_sleep(10ms); co_return 1; }());
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

    std::this_thread::sleep_for(25ms);

    auto [res, unit] = when_all(t1, t2).result();
    REQUIRE(res == 10);

    std::vector<Task<void>> done_voids{t2, t3};
    REQUIRE_NOTHROW(when_all(std::move(done_voids)).wait());
}
