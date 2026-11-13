#include <aw/tpl/cancellation.hpp>

namespace aw::tpl {

OperationCanceledException::OperationCanceledException()
    : std::runtime_error("Operation was canceled.") {}

namespace detail {

void CancellationState::cancel() {
    std::vector<std::function<void()>> to_run;
    {
        std::lock_guard lock(mtx);
        if (canceled.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        to_run.swap(callbacks);
    }
    for (auto& cb : to_run) {
        if (cb) cb();
    }
}

void CancellationState::register_callback(std::function<void()> cb) {
    bool run_now = false;
    {
        std::lock_guard lock(mtx);
        if (canceled.load(std::memory_order_acquire)) {
            run_now = true;
        } else {
            callbacks.push_back(std::move(cb));
        }
    }
    if (run_now && cb) {
        cb();
    }
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

void CancellationToken::register_callback(std::function<void()> cb) const {
    if (state_) {
        state_->register_callback(std::move(cb));
    }
}

CancellationToken::CancellationToken(std::shared_ptr<detail::CancellationState> state) noexcept
    : state_(std::move(state)) {}

CancellationTokenSource::CancellationTokenSource()
    : state_(std::make_shared<detail::CancellationState>()) {}

CancellationToken CancellationTokenSource::token() const noexcept {
    return CancellationToken{state_};
}

void CancellationTokenSource::cancel() {
    state_->cancel();
}

void CancellationTokenSource::cancel_after(std::chrono::milliseconds delay) {
    auto state = state_;

    struct CancelTimerBridge {
        struct promise_type {
            CancelTimerBridge get_return_object() noexcept { return {}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() noexcept {} // LCOV_EXCL_LINE

        };
    };

    [](std::shared_ptr<detail::CancellationState> st, std::chrono::milliseconds d) -> CancelTimerBridge {
        co_await async_sleep(d);
        st->cancel();
        co_return;
    }(std::move(state), delay);
}

bool CancelableSleepAwaiter::await_ready() const {
    token.throw_if_cancellation_requested();
    return duration.count() <= 0;
}

void CancelableSleepAwaiter::await_suspend(std::coroutine_handle<> h) const {
    auto try_resume = [flag = resumed, h]() {
        if (!flag->exchange(true, std::memory_order_acq_rel)) {
            detail::schedule_on_pool(h);
        }
    };

    token.register_callback(try_resume);

    struct TimerBridge {
        struct promise_type {
            TimerBridge get_return_object() noexcept { return {}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() noexcept {} // LCOV_EXCL_LINE
        };
    };

    [](std::chrono::milliseconds d, auto resume_fn) -> TimerBridge {
        co_await async_sleep(d);
        resume_fn();
        co_return;
    }(duration, try_resume);
}

void CancelableSleepAwaiter::await_resume() const {
    token.throw_if_cancellation_requested();
}

CancelableSleepAwaiter async_sleep(std::chrono::milliseconds duration, CancellationToken token) {
    return CancelableSleepAwaiter{duration, std::move(token)};
}

} // namespace aw::tpl
