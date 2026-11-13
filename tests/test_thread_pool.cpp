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

TEST_CASE("ThreadPool global singleton respects configured thread count", "[thread_pool]") {
    REQUIRE(ThreadPool::global().thread_count() >= 1);
}

TEST_CASE("async_sleep falls back to global pool when awaited from non-pool thread", "[thread_pool][fallback]") {
    std::atomic<bool> done{false};

    // Call from a raw std::thread where detail::current_pool is nullptr
    std::thread external_thread([&]() {
        auto sleeper = []() -> Task<int> {
            co_await async_sleep(15ms);
            co_return 123;
        };
        REQUIRE(sleeper().result() == 123);
        done.store(true);
    });

    external_thread.join();
    REQUIRE(done.load());
}

TEST_CASE("Coroutines spawned from raw std::thread fall back to global ThreadPool", "[thread_pool][fallback]") {
    int res = 0;
    std::thread t([&]() {
        auto coro = []() -> Task<int> {
            co_await async_sleep(10ms);
            co_return 42;
        };
        res = coro().result();
    });
    t.join();
    REQUIRE(res == 42);
}

TEST_CASE("ThreadPool destructor cleanly stops timer thread with pending timers", "[thread_pool][dtor]") {
    {
        ThreadPool temp_pool(1);
        // Schedule a long timer directly via a coroutine on global/temp pool and destroy immediately
        auto t = []() -> Task<void> {
            co_await async_sleep(5000ms);
            co_return;
        }();
        std::this_thread::sleep_for(10ms);
    }
    SUCCEED();
}

TEST_CASE("ThreadPool destructor interrupts active timer wait_until cleanly", "[thread_pool][dtor]") {
    {
        ThreadPool temp_pool(1);
        detail::current_pool = &temp_pool;

        // Enqueue a long timer directly onto temp_pool's timer thread
        auto awaiter = async_sleep(10000ms);
        awaiter.await_suspend(std::noop_coroutine());

        detail::current_pool = nullptr;
        std::this_thread::sleep_for(15ms); // Let timer_loop enter wait_until(next_wake)
    } // ~ThreadPool() triggers request_stop() while timer_loop is inside wait_until
    SUCCEED();
}

TEST_CASE("ThreadPool destructor cleanly aborts timer_loop during wait_until", "[thread_pool][dtor]") {
    {
        ThreadPool temp_pool(1);

        auto long_sleep = []() -> Task<void> {
            co_await async_sleep(10000ms);
            co_return;
        };

        detail::current_pool = &temp_pool;
        auto t = long_sleep();
        detail::current_pool = nullptr;

        std::this_thread::sleep_for(30ms);
    }
    SUCCEED();
}
