#pragma once

#include <aw/tpl/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <coroutine>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace aw::tpl {

/**
 * @class OperationCanceledException
 * @brief Exception thrown when an asynchronous operation is canceled via a @c CancellationToken.
 */
class OperationCanceledException : public std::runtime_error {
public:
    OperationCanceledException() : std::runtime_error("Operation was canceled.") {}
};

namespace detail {

/**
 * @struct CancellationState
 * @brief Shared thread-safe state backing a @c CancellationTokenSource and its tokens.
 */
struct CancellationState {
    std::atomic<bool> canceled{false};
    std::mutex mtx;
    std::vector<std::function<void()>> callbacks;

    /**
     * @brief Transitions the state to canceled and invokes all registered callbacks once.
     */
    void cancel() {
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

    /**
     * @brief Registers a callback to run upon cancellation, or executes it immediately if already canceled.
     * @param[in] cb Callable invoked on cancellation.
     */
    void register_callback(std::function<void()> cb) {
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
};

} // namespace detail

/**
 * @class CancellationToken
 * @brief Lightweight copyable token passed to async operations to observe cancellation requests
 *        (equivalent to C# @c System.Threading.CancellationToken).
 */
class CancellationToken {
public:
    CancellationToken() noexcept = default;

    /**
     * @brief Returns a default token that can never be canceled.
     * @return Non-cancelable @c CancellationToken.
     */
    static CancellationToken none() noexcept { return {}; }

    /**
     * @brief Checks whether cancellation has been requested.
     * @return True if canceled; false otherwise.
     */
    bool is_cancellation_requested() const noexcept {
        return state_ && state_->canceled.load(std::memory_order_acquire);
    }

    /**
     * @brief Throws @c OperationCanceledException if cancellation was requested.
     * @throws OperationCanceledException When token is canceled.
     */
    void throw_if_cancellation_requested() const {
        if (is_cancellation_requested()) {
            throw OperationCanceledException{};
        }
    }

    /**
     * @brief Registers a callback invoked when cancellation is triggered.
     * @param[in] cb Callable to invoke upon cancellation.
     */
    void register_callback(std::function<void()> cb) const {
        if (state_) {
            state_->register_callback(std::move(cb));
        }
    }

private:
    friend class CancellationTokenSource;

    explicit CancellationToken(std::shared_ptr<detail::CancellationState> state) noexcept
        : state_(std::move(state)) {}

    std::shared_ptr<detail::CancellationState> state_;
};

/**
 * @class CancellationTokenSource
 * @brief Signals cancellation to one or more @c CancellationToken instances
 *        (equivalent to C# @c System.Threading.CancellationTokenSource).
 */
class CancellationTokenSource {
public:
    CancellationTokenSource() : state_(std::make_shared<detail::CancellationState>()) {}

    /**
     * @brief Returns a @c CancellationToken linked to this source.
     * @return Associated @c CancellationToken.
     */
    CancellationToken token() const noexcept {
        return CancellationToken{state_};
    }

    /**
     * @brief Requests cancellation immediately and notifies all registered callbacks.
     */
    void cancel() {
        state_->cancel();
    }

    /**
     * @brief Schedules automatic cancellation after the specified duration.
     * @param[in] delay Time interval after which cancellation is triggered.
     */
    void cancel_after(std::chrono::milliseconds delay) {
        auto state = state_;

        struct CancelTimerBridge {
            struct promise_type {
                CancelTimerBridge get_return_object() noexcept { return {}; }
                std::suspend_never initial_suspend() noexcept { return {}; }
                std::suspend_never final_suspend() noexcept { return {}; }
                void return_void() noexcept {}
                void unhandled_exception() noexcept {}
            };
        };

        [](std::shared_ptr<detail::CancellationState> st, std::chrono::milliseconds d) -> CancelTimerBridge {
            co_await async_sleep(d);
            st->cancel();
        }(std::move(state), delay);
    }

private:
    std::shared_ptr<detail::CancellationState> state_;
};

/**
 * @brief Suspends the current coroutine asynchronously with cancellation support.
 * @param[in] duration Time interval to wait before resuming.
 * @param[in] token Cancellation token aborting the sleep early if triggered.
 * @return Awaitable timer object throwing @c OperationCanceledException if canceled.
 */
inline auto async_sleep(std::chrono::milliseconds duration, CancellationToken token) {
    struct CancelableSleepAwaiter {
        std::chrono::milliseconds duration;
        CancellationToken token;
        std::shared_ptr<std::atomic<bool>> resumed = std::make_shared<std::atomic<bool>>(false);

        bool await_ready() const {
            token.throw_if_cancellation_requested();
            return duration.count() <= 0;
        }

        void await_suspend(std::coroutine_handle<> h) const {
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
                    void unhandled_exception() noexcept {}
                };
            };

            [](std::chrono::milliseconds d, auto resume_fn) -> TimerBridge {
                co_await async_sleep(d);
                resume_fn();
            }(duration, try_resume);
        }

        void await_resume() const {
            token.throw_if_cancellation_requested();
        }
    };

    return CancelableSleepAwaiter{duration, std::move(token)};
}

} // namespace aw::tpl