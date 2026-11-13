#pragma once

#include <coroutine>
#include <exception>
#include <atomic>
#include <mutex>
#include <optional>
#include <semaphore>
#include <vector>
#include <aw/tpl/thread_pool.hpp>

namespace aw::tpl::threading::tasks {

/**
 * @struct TaskStateBase
 * @brief Shared synchronization state between a running coroutine and its @c Task handle(s).
 */
struct TaskStateBase {
    std::mutex mtx;
    bool ready = false;
    std::vector<std::coroutine_handle<>> continuations;
    std::exception_ptr exception = nullptr;
    std::binary_semaphore done_sem{0};

    /**
     * @brief Checks whether the task has completed in a thread-safe manner.
     * @return True if result or exception is ready.
     */
    bool is_ready() {
        std::lock_guard lock(mtx);
        return ready;
    }

    /**
     * @brief Registers an awaiting coroutine if the task is not yet complete.
     * @param[in] awaiting Handle of the caller coroutine.
     * @return True if suspended and @p awaiting will be resumed later; false if already ready.
     */
    bool try_attach_continuation(std::coroutine_handle<> awaiting) {
        std::lock_guard lock(mtx);
        if (ready) {
            return false;
        }
        continuations.push_back(awaiting);
        return true;
    }

    /**
     * @brief Finishes the task state, releasing waiters, and returning the first continuation handle.
     * @return The first continuation handle, or std::noop_coroutine() if no continuations were attached.
     */
    std::coroutine_handle<> finish() {
        std::vector<std::coroutine_handle<>> to_resume;
        {
            std::lock_guard lock(mtx);
            ready = true;
            to_resume = std::move(continuations);
            continuations.clear();
        }
        done_sem.release();

        if (to_resume.empty()) {
            return std::noop_coroutine();
        }

        // The first continuation is resumed by symmetric transfer, the rest are scheduled on the pool.
        std::coroutine_handle<> first = to_resume.front();
        for (size_t i = 1; i < to_resume.size(); ++i) {
            // Schedule the remaining waiters on the pool
            ::aw::tpl::detail::schedule_on_pool(to_resume[i]);
        }
        return first;
    }

    /**
     * @brief Marks the task as finished, releases blocking waiters, and resumes the first continuation.
     */
    void complete() {
        // Use finish() to perform cleanup and get the handle for symmetric transfer.
        std::coroutine_handle<> first = finish();
        first.resume();
    }
};

/**
 * @struct TaskState
 * @brief Shared state specialization storing a return value of type @p T.
 * @tparam T Result type produced by the coroutine.
 */
template <typename T>
struct TaskState : TaskStateBase {
    std::optional<T> value;
    std::atomic<bool> value_taken{false};

    /**
     * @brief One-shot claim used when a move-only result is extracted.
     * @return True for the first caller only.
     */
    bool try_take() noexcept {
        return !value_taken.exchange(true, std::memory_order_acq_rel);
    }
};

/**
 * @struct TaskState<void>
 * @brief Shared state specialization for void-returning tasks.
 */
template <>
struct TaskState<void> : TaskStateBase {};

} // namespace aw::tpl::threading::tasks