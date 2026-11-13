#include <aw/tpl/threading/tasks/detail/when_any_awaiter.hpp>

namespace aw::tpl::threading::tasks::detail {

bool WhenAnyAwaiter::await_ready() const noexcept {
    return state->completed.load(std::memory_order_acquire);
}

std::coroutine_handle<> WhenAnyAwaiter::await_suspend(std::coroutine_handle<> h) {
    std::lock_guard lock(state->mtx);
    // Reading winner_index later is race free: it is written before completed is set (see try_complete).
    if (state->completed.load(std::memory_order_acquire)) {
        return h;
    }
    state->waiter = h;
    return std::noop_coroutine();
}

std::size_t WhenAnyAwaiter::await_resume() const noexcept {
    return state->winner_index;
}

} // namespace aw::tpl::threading::tasks::detail
