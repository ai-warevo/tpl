#pragma once

#include <aw/tpl/threading/tasks/detail/task_state_base.hpp>

#include <coroutine>
#include <memory>

namespace aw::tpl::threading::tasks::detail {

/**
 * @struct CompletionAwaiter
 * @brief Awaitable that only waits for completion: it neither rethrows the task's exception nor
 *        extracts (moves out) the result. Returned by @c Task::completion().
 */
struct CompletionAwaiter {
    std::shared_ptr<TaskStateBase> state;

    bool await_ready() const;
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) const;
    void await_resume() const noexcept {}
};

} // namespace aw::tpl::threading::tasks::detail
