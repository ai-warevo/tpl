#pragma once

#include <coroutine>
#include <exception>
#include <mutex>
#include <semaphore>

namespace aw::tpl::threading::tasks {

/**
 * @struct TaskStateBase
 * @brief Shared synchronization state between a running coroutine and its @c Task handle(s).
 */
struct TaskStateBase {
    std::mutex mtx;
    bool ready = false;
    std::coroutine_handle<> continuation = nullptr;
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
        continuation = awaiting;
        return true;
    }

    /**
     * @brief Marks the task as finished, releases blocking waiters, and resumes any continuation.
     */
    void complete() {
        std::coroutine_handle<> cont = nullptr;
        {
            std::lock_guard lock(mtx);
            ready = true;
            cont = continuation;
        }
        done_sem.release();
        if (cont) {
            cont.resume();
        }
    }
};

/**
 * @struct TaskState
 * @brief Shared state specialization storing a return value of type @p T.
 * @tparam T Result type produced by the coroutine.
 */
template <typename T>
struct TaskState : TaskStateBase {
    T value{};
};

/**
 * @struct TaskState<void>
 * @brief Shared state specialization for void-returning tasks.
 */
template <>
struct TaskState<void> : TaskStateBase {};


} // namespace aw::tpl::threading::tasks
