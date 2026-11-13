#include <aw/tpl/threading/cancellation_token_source.hpp>

#include <aw/tpl/threading/detail/cancellation_state.hpp>
#include <aw/tpl/threading/detail/delay_awaiter.hpp>
#include <aw/tpl/threading/detail/detached.hpp>

#include <utility>

namespace aw::tpl::threading {

CancellationTokenSource::CancellationTokenSource()
    : state_(std::make_shared<detail::CancellationState>()) {}

CancellationTokenSource::CancellationTokenSource(std::chrono::milliseconds delay)
    : CancellationTokenSource() {
    cancel_after(delay);
}

CancellationToken CancellationTokenSource::token() const noexcept {
    // A moved-from source has no state; it hands out a default token that can never be canceled.
    return CancellationToken{state_};
}

void CancellationTokenSource::cancel() {
    if (!state_) return; // moved-from source is inert
    state_->cancel();
}

void CancellationTokenSource::cancel_after(std::chrono::milliseconds delay) {
    if (!state_) return; // moved-from source is inert
    auto state = state_;

    [](std::shared_ptr<detail::CancellationState> st, std::chrono::milliseconds d) -> detail::Detached {
        co_await detail::DelayAwaiter{d};
        try {
            st->cancel();
        } catch (...) {
            // A throwing user callback must not take the timer coroutine down; all callbacks already ran.
        }
        co_return;
    }(std::move(state), delay).start();
}

} // namespace aw::tpl::threading
