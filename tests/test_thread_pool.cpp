#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/tpl.hpp>

#include <atomic>
#include <chrono>
#include <vector>

using namespace std::chrono_literals;
using namespace aw::tpl;

TEST_CASE("ThreadPool handles zero thread count fallback and explicit sizing", "[thread_pool]") {
    ThreadPool::set_default_threads(0); // Should be ignored
    ThreadPool::set_default_threads(4);

    ThreadPool custom_pool(0); // Should fallback to 2 threads
    REQUIRE(custom_pool.thread_count() == 2);

    ThreadPool explicit_pool(3);
    REQUIRE(explicit_pool.thread_count() == 3);
}

TEST_CASE("async_sleep completes immediately for zero or negative durations", "[thread_pool][sleep]") {
    auto zero_sleep = []() -> Task<int> {
        co_await async_sleep(0ms);
        co_await async_sleep(-50ms);
        co_return 100;
    };

    REQUIRE(zero_sleep().result() == 100);
}

TEST_CASE("Timer queue orders earlier deadlines ahead of later ones", "[thread_pool][timer]") {
    std::atomic<int> order{0};
    int first_finished = 0;
    int second_finished = 0;

    auto timed_worker = [](std::chrono::milliseconds delay, std::atomic<int>& ord, int& out_pos) -> Task<void> {
        co_await async_sleep(delay);
        out_pos = ++ord;
        co_return;
    };

    auto t1 = timed_worker(150ms, order, first_finished);
    std::this_thread::sleep_for(10ms); // Ensure t1 is already sleeping in timer_loop
    auto t2 = timed_worker(30ms, order, second_finished); // Should wake up timer_cv_ and execute before t1

    when_all(t1, t2).wait();

    REQUIRE(second_finished == 1);
    REQUIRE(first_finished == 2);
}
