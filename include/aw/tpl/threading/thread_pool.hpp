#pragma once

#include <aw/tpl/threading/detail/timer_entry.hpp>

#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>

namespace aw::tpl::threading {

class ThreadPool;

namespace detail {

void schedule_on_pool(ThreadPool* pool, std::coroutine_handle<> h);
void schedule_after_on_pool(ThreadPool* pool, std::coroutine_handle<> h, std::chrono::milliseconds delay);

} // namespace detail

/**
 * @class ThreadPool
 * @brief Global-capable C++20 thread pool with worker threads and a non-blocking async timer thread
 *        (equivalent to C# @c System.Threading.ThreadPool).
 */
class ThreadPool {
public:
    /**
     * @brief Configures the worker thread count used when the global pool is first initialized.
     *        Must be called before the first call to @c ThreadPool::global() or any coroutine launch;
     *        it has no effect on the global pool once that pool has been created.
     * @param[in] count Desired number of worker threads (ignored if 0).
     */
    static void set_default_threads(std::size_t count) noexcept;

    /**
     * @brief Returns the process-wide default thread pool instance (similar to C# ThreadPool).
     * @return Reference to the global @c ThreadPool singleton.
     */
    static ThreadPool& global();

    /**
     * @brief Constructs a pool with the configured default worker thread count.
     */
    ThreadPool();

    /**
     * @brief Constructs the thread pool and spawns worker threads plus a dedicated timer thread.
     * @param[in] thread_count Number of worker threads to spawn (0 falls back to 2).
     */
    explicit ThreadPool(std::size_t thread_count);

    /**
     * @brief Stops the timer thread, resumes pending timers immediately, drains queued work, and joins all threads.
     */
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief Returns the number of active worker threads in the pool (excluding the timer thread).
     * @return Count of worker threads.
     */
    [[nodiscard]] std::size_t thread_count() const noexcept;

private:
    friend void detail::schedule_on_pool(ThreadPool* pool, std::coroutine_handle<> h);
    friend void detail::schedule_after_on_pool(ThreadPool* pool, std::coroutine_handle<> h, std::chrono::milliseconds delay);

    /// Enqueues a coroutine handle to be resumed on one of the worker threads.
    void schedule(std::coroutine_handle<> h);

    /// Schedules a coroutine to resume after the specified duration without blocking workers.
    void schedule_after(std::coroutine_handle<> h, std::chrono::milliseconds duration);

    /// Main execution loop for worker threads.
    void worker_loop(std::stop_token st);

    /// Dedicated loop managing pending timers and dispatching expired coroutines to workers.
    void timer_loop(std::stop_token st);

    /**
     * @brief Ordered shutdown shared by the destructor and the constructor failure path:
     *        stops and joins the timer thread, flushes pending timers into the queue, then stops
     *        and joins the workers (which drain the queue first).
     */
    void shutdown() noexcept;

    // Synchronization primitives and queues come first; the threads are declared LAST so that
    // they are destroyed (and joined) FIRST, while everything they use is still alive.
    std::queue<std::coroutine_handle<>> tasks_;
    std::mutex queue_mtx_;
    std::condition_variable queue_cv_;

    std::priority_queue<detail::TimerEntry, std::vector<detail::TimerEntry>, std::greater<>> timers_;
    std::mutex timer_mtx_;
    std::condition_variable timer_cv_;

    std::vector<std::jthread> workers_;
    std::jthread timer_thread_;
};

} // namespace aw::tpl::threading
