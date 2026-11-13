#include <aw/tpl/cancellation.hpp>

#include <exception>

namespace aw::tpl {

OperationCanceledException::OperationCanceledException()
    : std::runtime_error("Operation was canceled.") {}

namespace detail {

void CancellationState::cancel() {
    std::vector<std::pair<std::uint64_t, std::function<void()>>> to_run;
    {
        std::lock_guard lock(mtx);
        if (canceled.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        to_run.swap(callbacks);
    }
    // The lock is not held while callbacks run. A throwing callback must not prevent the others
    // from running (they may resume sleeping coroutines), so remember the first error and rethrow it.
    std::exception_ptr first_error;
    for (auto& entry : to_run) {
        if (!entry.second) continue;
        try {
            entry.second();
        } catch (...) {
            if (!first_error) first_error = std::current_exception();
        }
    }
    if (first_error) std::rethrow_exception(first_error);
}

std::uint64_t CancellationState::register_callback(std::function<void()> cb) {
    std::uint64_t id = 0;
    bool run_now = false;
    {
        std::lock_guard lock(mtx);
        if (canceled.load(std::memory_order_acquire)) {
            run_now = true;
        } else {
            id = next_id++;
            callbacks.emplace_back(id, std::move(cb));
        }
    }
    // An exception from an immediately executed callback propagates to the caller.
    if (run_now && cb) {
        cb();
    }
    return id;
}

bool CancellationState::unregister_callback(std::uint64_t id) {
    if (id == 0) return false;
    std::lock_guard lock(mtx);
    for (auto it = callbacks.begin(); it != callbacks.end(); ++it) {
        if (it->first == id) {
            callbacks.erase(it);
            return true;
        }
    }
    return false;
}

std::size_t CancellationState::callback_count() {
    std::lock_guard lock(mtx);
    return callbacks.size();
}

} // namespace detail

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

CancellationTokenSource::CancellationTokenSource()
    : state_(std::make_shared<detail::CancellationState>()) {}

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

    struct CancelTimerBridge {
        struct promise_type {
            CancelTimerBridge get_return_object() noexcept { return {}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            // Failing fast is preferable to a silent hang of whoever waits on the token.
            void unhandled_exception() noexcept { std::terminate(); } // LCOV_EXCL_LINE
        };
    };

    [](std::shared_ptr<detail::CancellationState> st, std::chrono::milliseconds d) -> CancelTimerBridge {
        co_await async_sleep(d);
        try {
            st->cancel();
        } catch (...) {
            // A throwing user callback must not take the timer coroutine down; all callbacks already ran.
        }
        co_return;
    }(std::move(state), delay);
}

bool CancelableSleepAwaiter::await_ready() const {
    token.throw_if_cancellation_requested();
    return duration.count() <= 0;
}

void CancelableSleepAwaiter::await_suspend(std::coroutine_handle<> h) const {
    // Copy everything needed into locals first: once the handle can be resumed by another thread
    // the coroutine frame (which owns this awaiter) may be destroyed, so `this` must not be used
    // after the timer/callback have been armed. A fresh flag per suspension also makes the same
    // awaiter object safely awaitable more than once.
    struct Control {
        std::atomic<bool> resumed{false};
        std::atomic<std::uint64_t> callback_id{0};
    };

    const auto d = duration;
    CancellationToken local_token = token;
    // Resume on the pool the coroutine was running on, not on whatever thread fires the cancel.
    ThreadPool* origin_pool = detail::current_pool;
    auto ctl = std::make_shared<Control>();

    // Cancellation path: the callback has already been consumed by cancel(), nothing to unregister.
    auto try_resume = [ctl, h, origin_pool]() {
        if (!ctl->resumed.exchange(true, std::memory_order_acq_rel)) {
            detail::schedule_on_pool(origin_pool, h);
        }
    };

    struct TimerBridge {
        struct promise_type {
            TimerBridge get_return_object() noexcept { return {}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() noexcept { std::terminate(); } // LCOV_EXCL_LINE
        };
    };

    // Arm the timer first, then the cancellation callback (which may run inline if already canceled).
    [](std::chrono::milliseconds delay, std::shared_ptr<Control> c, CancellationToken tok, std::coroutine_handle<> handle, ThreadPool* pool) -> TimerBridge {
        co_await async_sleep(delay);
        if (!c->resumed.exchange(true, std::memory_order_acq_rel)) {
            // Timer won: drop the now useless cancel callback so a long-lived token does not grow.
            // Done BEFORE resuming the coroutine, using only copies owned by this bridge frame.
            (void)tok.unregister_callback(c->callback_id.load());
            detail::schedule_on_pool(pool, handle);
        }
        co_return;
    }(d, ctl, local_token, h, origin_pool);

    const auto id = local_token.register_callback(try_resume);
    ctl->callback_id.store(id);
    // The timer may have won before the id was published; then it could not unregister, so do it here.
    if (id != 0 && ctl->resumed.load()) {
        (void)local_token.unregister_callback(id);
    }
}

void CancelableSleepAwaiter::await_resume() const {
    token.throw_if_cancellation_requested();
}

CancelableSleepAwaiter async_sleep(std::chrono::milliseconds duration, CancellationToken token) {
    return CancelableSleepAwaiter{duration, std::move(token)};
}

} // namespace aw::tpl
