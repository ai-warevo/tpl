#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/tpl.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading::tasks;

namespace {

struct CurrentPoolGuard {
    ThreadPool* previous;
    explicit CurrentPoolGuard(ThreadPool* p) : previous(detail::current_pool) { detail::current_pool = p; }
    ~CurrentPoolGuard() { detail::current_pool = previous; }
};

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
    int result = 0;

    // Call from a raw std::thread where detail::current_pool is nullptr
    std::thread external_thread([&]() {
        auto sleeper = []() -> Task<int> {
            co_await async_sleep(15ms);
            co_return 123;
        };
        result = sleeper().result();
        done.store(true);
    });

    external_thread.join();
    REQUIRE(done.load());
    REQUIRE(result == 123);
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
    Task<void> t;
    {
        ThreadPool temp_pool(1);
        CurrentPoolGuard guard(&temp_pool); // make the coroutine really run on temp_pool
        t = []() -> Task<void> {
            co_await async_sleep(5000ms);
            co_return;
        }();
        std::this_thread::sleep_for(10ms); // give the coroutine time to arm its timer
    }
    // Destruction resumed the pending timer immediately.
    REQUIRE(wait_until([&] { return t.await_ready(); }));
}

TEST_CASE("ThreadPool destructor interrupts active timer wait_until cleanly", "[thread_pool][dtor]") {
    {
        ThreadPool temp_pool(1);
        {
            CurrentPoolGuard guard(&temp_pool);
            // Enqueue a long timer directly onto temp_pool's timer thread
            auto awaiter = async_sleep(10000ms);
            awaiter.await_suspend(std::noop_coroutine());
        }
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

        Task<void> t;
        {
            CurrentPoolGuard guard(&temp_pool);
            t = long_sleep();
        }

        std::this_thread::sleep_for(30ms);
    }
    SUCCEED();
}

// --- Pool shutdown (tasks 04-06) ---

namespace {

Task<void> sleep_then_bump(std::chrono::milliseconds d, std::atomic<int>& counter) {
    co_await async_sleep(d);
    ++counter;
}

Task<int> sleep_twice_return(int v) {
    co_await async_sleep(5000ms);
    co_await async_sleep(5000ms);
    co_return v;
}

Task<int> sleep_return(int v) {
    co_await async_sleep(5000ms);
    co_return v;
}

} // namespace

TEST_CASE("ThreadPool can be created and destroyed repeatedly with pending work", "[thread_pool][dtor][stress]") {
    std::atomic<int> counter{0};
    for (int i = 0; i < 300; ++i) {
        ThreadPool pool(2);
        CurrentPoolGuard guard(&pool);
        auto long_task = sleep_then_bump(10s, counter);
        auto short_task = sleep_then_bump(1ms, counter);
        (void)long_task;
        (void)short_task;
    }
    SUCCEED();
}

TEST_CASE("Pool destruction resumes coroutines suspended on timers", "[thread_pool][dtor][flush]") {
    Task<int> task;
    {
        ThreadPool pool(1);
        CurrentPoolGuard guard(&pool);
        task = sleep_return(5);
        std::this_thread::sleep_for(30ms); // let the coroutine reach its timer
    }
    REQUIRE(task.result() == 5);
}

TEST_CASE("Pool destruction drains queued work", "[thread_pool][dtor][flush]") {
    std::atomic<int> counter{0};
    std::vector<Task<void>> tasks;
    {
        ThreadPool pool(1);
        CurrentPoolGuard guard(&pool);
        for (int i = 0; i < 100; ++i) {
            tasks.push_back(sleep_then_bump(0ms, counter));
        }
    }
    REQUIRE(counter.load() == 100);
}

TEST_CASE("Sleep started during shutdown completes immediately", "[thread_pool][dtor]") {
    Task<int> task;
    {
        ThreadPool pool(1);
        CurrentPoolGuard guard(&pool);
        task = sleep_twice_return(9);
        std::this_thread::sleep_for(30ms);
    }
    REQUIRE(task.result() == 9);
}

// --- Task 18: constructor / sizes ---

TEST_CASE("ThreadPool sizes 1 and 8 construct and run work", "[thread_pool]") {
    for (size_t n : {size_t{1}, size_t{8}}) {
        std::atomic<int> counter{0};
        std::vector<Task<void>> tasks;
        {
            ThreadPool pool(n);
            REQUIRE(pool.thread_count() == n);
            CurrentPoolGuard guard(&pool);
            for (int i = 0; i < 20; ++i) tasks.push_back(sleep_then_bump(0ms, counter));
        }
        REQUIRE(counter.load() == 20);
    }
}

// --- Task 19: timer overflow ---

TEST_CASE("async_sleep with maximum duration does not fire immediately", "[thread_pool][sleep][overflow]") {
    Task<void> task;
    {
        ThreadPool pool(1);
        CurrentPoolGuard guard(&pool);
        task = []() -> Task<void> { co_await async_sleep(std::chrono::milliseconds::max()); }();
        std::this_thread::sleep_for(100ms);
        REQUIRE_FALSE(task.await_ready());
    } // pool destruction wakes the pending timer
    task.wait();
}

// --- Task 33: default thread count ---

TEST_CASE("set_default_threads after global pool creation is ignored", "[thread_pool]") {
    const size_t before = ThreadPool::global().thread_count();
    ThreadPool::set_default_threads(before + 3);
    REQUIRE(ThreadPool::global().thread_count() == before);
}

// --- Tasks 34-35: pool affinity ---

namespace {

struct ScheduleOnPoolAwaiter {
    ThreadPool* target;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) const { detail::schedule_on_pool(target, h); }
    void await_resume() const noexcept {}
};

Task<ThreadPool*> hop_to(ThreadPool* target) {
    co_await ScheduleOnPoolAwaiter{target};
    co_return detail::current_pool;
}

} // namespace

TEST_CASE("schedule_on_pool targets the given pool", "[thread_pool][affinity]") {
    ThreadPool pool(1);
    SECTION("explicit pool") {
        REQUIRE(hop_to(&pool).result() == &pool);
    }
    SECTION("null pool falls back to current/global pool") {
        ThreadPool* resumed_on = hop_to(nullptr).result();
        REQUIRE(resumed_on != nullptr);
        REQUIRE(resumed_on != &pool);
    }
    REQUIRE(detail::current_pool == nullptr);
}
