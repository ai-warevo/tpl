#pragma once

#include <coroutine>
#include <exception>
#include <mutex>
#include <semaphore>
#include <vector>

namespace aw::tpl::threading::tasks::detail {

/**
 * @brief Throws @c std::logic_error: the task has no shared state (default-constructed or moved-from).
 */
[[noreturn]] void throw_task_has_no_state();

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

    /// Coroutine frame owned by this state once the coroutine reached its final suspend point.
    std::coroutine_handle<> frame = nullptr;

    TaskStateBase() = default;
    TaskStateBase(const TaskStateBase&) = delete;
    TaskStateBase& operator=(const TaskStateBase&) = delete;

    /**
     * @brief Destroys the finished coroutine frame (and with it any by-value parameters).
     *        Runs when the last Task/awaiter reference is dropped, i.e. never while the ramp function
     *        of the coroutine can still be running.
     */
    ~TaskStateBase();

    /**
     * @brief Checks whether the task has completed in a thread-safe manner.
     * @return True if result or exception is ready.
     */
    [[nodiscard]] bool is_ready();

    /**
     * @brief Registers an awaiting coroutine if the task is not yet complete.
     * @param[in] awaiting Handle of the caller coroutine.
     * @return True if suspended and @p awaiting will be resumed later; false if already ready.
     */
    bool try_attach_continuation(std::coroutine_handle<> awaiting);

    /**
     * @brief @c await_suspend helper: attaches @p awaiting or hands it straight back when already ready.
     * @return @c std::noop_coroutine() if suspended; @p awaiting if it must continue immediately.
     */
    std::coroutine_handle<> suspend_or_resume(std::coroutine_handle<> awaiting);

    /**
     * @brief Finishes the task state, releasing waiters, and returning the first continuation handle.
     * @return The first continuation handle, or std::noop_coroutine() if no continuations were attached.
     */
    std::coroutine_handle<> finish();

    /**
     * @brief Marks the task as finished, releases blocking waiters, and resumes the first continuation.
     */
    void complete();

    /**
     * @brief Blocks the calling thread until the task is finished.
     * @throws std::logic_error When called on a pool worker for an unfinished task (would deadlock).
     */
    void wait_blocking();
};

} // namespace aw::tpl::threading::tasks::detail
