#pragma once

#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>

namespace aw::tpl {

class ThreadPool;

namespace detail {

/**
 * @brief Thread-local pointer referencing the ThreadPool executing on the current worker thread.
 */
inline thread_local ThreadPool* current_pool = nullptr;

/**
 * @brief Internal bridge scheduling a coroutine handle onto the active or global @c ThreadPool.
 * @param[in] h Coroutine handle to enqueue.
 */
void schedule_on_pool(std::coroutine_handle<> h);

struct InitialScheduleAwaiter;

} // namespace detail

inline auto async_sleep(std::chrono::milliseconds duration);

/**
 * @class ThreadPool
 * @brief Global-capable C++20 thread pool with worker threads and a non-blocking async timer thread.
 */
class ThreadPool {
public:
    /**
     * @brief Configures the worker thread count used when the global pool is first initialized.
     *        Must be called before the first call to @c ThreadPool::global() or any coroutine launch.
     * @param[in] count Desired number of worker threads (ignored if 0).
     */
    static void set_default_threads(size_t count) noexcept;

    /**
     * @brief Returns the process-wide default thread pool instance (similar to C# ThreadPool).
     * @return Reference to the global @c ThreadPool singleton.
     */
    static ThreadPool& global();

    /**
     * @brief Constructs the thread pool and spawns worker threads plus a dedicated timer thread.
     * @param[in] thread_count Number of worker threads to spawn (defaults to @c default_thread_count_).
     */
    explicit ThreadPool(size_t thread_count = default_thread_count_);

    /**
     * @brief Stops all worker and timer threads and joins them safely.
     */
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief Returns the number of active worker threads in the pool (excluding the timer thread).
     * @return Count of worker threads.
     */
    size_t thread_count() const noexcept;

private:
    friend void detail::schedule_on_pool(std::coroutine_handle<> h);
    friend struct detail::InitialScheduleAwaiter;
    friend auto async_sleep(std::chrono::milliseconds duration);

    /**
     * @struct TimerEntry
     * @brief Represents a scheduled timer event ordered by expiration time.
     */
    struct TimerEntry {
        std::chrono::steady_clock::time_point wake_time; ///< Timestamp when the coroutine should resume.
        std::coroutine_handle<> handle;                  ///< Suspended coroutine handle.

        bool operator>(const TimerEntry& other) const noexcept {
            return wake_time > other.wake_time;
        }
    };

    /**
     * @brief Enqueues a coroutine handle to be resumed on one of the worker threads.
     * @param[in] h Coroutine handle to schedule for execution.
     */
    void schedule(std::coroutine_handle<> h);

    /**
     * @brief Schedules a coroutine to resume after the specified duration without blocking workers.
     * @param[in] h Coroutine handle to suspend until expiration.
     * @param[in] duration Time interval to wait before resuming.
     */
    void schedule_after(std::coroutine_handle<> h, std::chrono::milliseconds duration);

    /**
     * @brief Main execution loop for worker threads binding @c detail::current_pool.
     * @param[in] st Stop token signaling thread termination.
     */
    void worker_loop(std::stop_token st);

    /**
     * @brief Dedicated loop managing pending timers and dispatching expired coroutines to workers.
     * @param[in] st Stop token signaling thread termination.
     */
    void timer_loop(std::stop_token st);

    static inline size_t default_thread_count_ = std::thread::hardware_concurrency();

    std::vector<std::jthread> workers_;
    std::queue<std::coroutine_handle<>> tasks_;
    std::mutex queue_mtx_;
    std::condition_variable queue_cv_;

    std::jthread timer_thread_;
    std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<>> timers_;
    std::mutex timer_mtx_;
    std::condition_variable timer_cv_;
};

/**
 * @brief Suspends the current coroutine asynchronously for the specified duration (like C# @c Task.Delay).
 * @param[in] duration Time interval to wait before resuming the coroutine.
 * @return Awaitable timer object bound to the active or global @c ThreadPool.
 */
inline auto async_sleep(std::chrono::milliseconds duration) {
    struct AsyncSleepAwaiter {
        std::chrono::milliseconds duration;

        bool await_ready() const noexcept { return duration.count() <= 0; }

        void await_suspend(std::coroutine_handle<> h) const {
            ThreadPool* pool = detail::current_pool ? detail::current_pool : &ThreadPool::global();
            pool->schedule_after(h, duration);
        }

        void await_resume() const noexcept {}
    };
    return AsyncSleepAwaiter{duration};
}

} // namespace aw::tpl
