#include <aw/tpl/threading/tasks/detail/completion_awaiter.hpp>

namespace aw::tpl::threading::tasks::detail {

bool CompletionAwaiter::await_ready() const {
    return !state || state->is_ready();
}

std::coroutine_handle<> CompletionAwaiter::await_suspend(std::coroutine_handle<> awaiting) const {
    return state->suspend_or_resume(awaiting);
}

} // namespace aw::tpl::threading::tasks::detail
