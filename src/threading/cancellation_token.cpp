#include <aw/tpl/threading/cancellation_token.hpp>

#include <aw/tpl/operation_canceled_exception.hpp>
#include <aw/tpl/threading/detail/cancellation_state.hpp>

#include <utility>

namespace aw::tpl::threading {

CancellationToken CancellationToken::none() noexcept {
    return {};
}

bool CancellationToken::is_cancellation_requested() const noexcept {
    return state_ && state_->canceled.load(std::memory_order_acquire);
}

void CancellationToken::throw_if_cancellation_requested() const {
    if (is_cancellation_requested()) {
        throw OperationCanceledException{};
    }
}

std::uint64_t CancellationToken::register_callback(std::function<void()> cb) const {
    if (state_) {
        return state_->register_callback(std::move(cb));
    }
    return 0;
}

bool CancellationToken::unregister_callback(std::uint64_t id) const {
    return state_ && state_->unregister_callback(id);
}

std::size_t CancellationToken::registered_callback_count() const {
    return state_ ? state_->callback_count() : 0;
}

CancellationToken::CancellationToken(std::shared_ptr<detail::CancellationState> state) noexcept
    : state_(std::move(state)) {}

} // namespace aw::tpl::threading
