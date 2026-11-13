#include <aw/tpl/threading/tasks/task_void.hpp>

#include <exception>
#include <utility>

namespace aw::tpl::threading::tasks {

Task<void>::Task(std::shared_ptr<detail::TaskState<void>> state) noexcept
    : state_(std::move(state)) {}

bool Task<void>::await_ready() const {
    return !state_ || state_->is_ready();
}

std::coroutine_handle<> Task<void>::await_suspend(std::coroutine_handle<> awaiting) {
    return state_->suspend_or_resume(awaiting);
}

void Task<void>::await_resume() const {
    if (!state_) {
        detail::throw_task_has_no_state();
    }
    if (state_->exception) {
        std::rethrow_exception(state_->exception);
    }
}

detail::CompletionAwaiter Task<void>::completion() const noexcept {
    return detail::CompletionAwaiter{state_};
}

void Task<void>::wait() const {
    if (!state_) {
        detail::throw_task_has_no_state();
    }
    state_->wait_blocking();
    await_resume();
}

} // namespace aw::tpl::threading::tasks
