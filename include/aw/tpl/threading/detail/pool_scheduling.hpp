#pragma once

#include <chrono>
#include <coroutine>

namespace aw::tpl::threading {

class ThreadPool;

} // namespace aw::tpl::threading

namespace aw::tpl::threading::detail {

/**
 * @brief The pool executing on the calling thread, or @c nullptr when called from a non-worker thread.
 */
[[nodiscard]] ThreadPool* current_pool() noexcept;

/**
 * @brief RAII helper that makes @p pool the "current pool" of the calling thread and restores the
 *        previous one on destruction. Used by workers; handy to simulate a worker in tests.
 */
class CurrentPoolScope {
public:
    /// Makes @p pool the calling thread's current pool until the scope ends.
    explicit CurrentPoolScope(ThreadPool* pool) noexcept;
    ~CurrentPoolScope() noexcept;

    CurrentPoolScope(const CurrentPoolScope&) = delete;
    CurrentPoolScope& operator=(const CurrentPoolScope&) = delete;

private:
    ThreadPool* previous_;
};

/**
 * @brief Schedules a coroutine handle onto the active or global @c ThreadPool.
 * @param[in] h Coroutine handle to enqueue.
 */
void schedule_on_pool(std::coroutine_handle<> h);

/**
 * @brief Schedules a coroutine handle on a specific pool.
 * @param[in] pool Target pool; if null the behaviour of the one-argument overload applies.
 * @param[in] h Coroutine handle to enqueue.
 */
void schedule_on_pool(ThreadPool* pool, std::coroutine_handle<> h);

/**
 * @brief Resumes @p h on @p pool (or the active/global pool if null) after @p delay without blocking a worker.
 */
void schedule_after_on_pool(ThreadPool* pool, std::coroutine_handle<> h, std::chrono::milliseconds delay);

} // namespace aw::tpl::threading::detail
