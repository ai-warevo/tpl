#pragma once

#include <aw/tpl/threading/tasks/detail/task_state_base.hpp>

#include <coroutine>
#include <memory>

namespace aw::tpl::threading::tasks::detail {

/**
 * @struct FinalAwaiter
 * @brief Awaiter run at final suspension. It hands ownership of the coroutine frame to the shared
 *        state (which destroys it when the LAST reference to the state is dropped), releases
 *        waiters, and resumes the first awaiting coroutine through symmetric transfer.
 *
 * The frame is deliberately NOT destroyed here. The creating thread's ramp function may still be
 * touching the frame (GCC 15 keeps a non-atomic counter in it that is updated after the handle was
 * scheduled), and the ramp's returned @c Task keeps the state alive until the ramp has returned.
 * Coroutine locals are already destroyed at this point; only by-value parameters live longer.
 */
struct FinalAwaiter {
    std::shared_ptr<TaskStateBase> st; ///< The promise's own reference, moved out in final_suspend().

    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept;
    void await_resume() const noexcept {}
};

} // namespace aw::tpl::threading::tasks::detail
