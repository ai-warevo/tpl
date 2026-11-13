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
    OperationCanceledException();
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
    void cancel();

    /**
     * @brief Registers a callback to run upon cancellation, or executes it immediately if already canceled.
     * @param[in] cb Callable invoked on cancellation.
     */
    void register_callback(std::function<void()> cb);
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
    static CancellationToken none() noexcept;

    /**
     * @brief Checks whether cancellation has been requested.
     * @return True if canceled; false otherwise.
     */
    bool is_cancellation_requested() const noexcept;

    /**
     * @brief Throws @c OperationCanceledException if cancellation was requested.
     * @throws OperationCanceledException When token is canceled.
     */
    void throw_if_cancellation_requested() const;

    /**
     * @brief Registers a callback invoked when cancellation is triggered.
     * @param[in] cb Callable to invoke upon cancellation.
     */
    void register_callback(std::function<void()> cb) const;

private:
    friend class CancellationTokenSource;

    explicit CancellationToken(std::shared_ptr<detail::CancellationState> state) noexcept;

    std::shared_ptr<detail::CancellationState> state_;
};

/**
 * @class CancellationTokenSource
 * @brief Signals cancellation to one or more @c CancellationToken instances
 *        (equivalent to C# @c System.Threading.CancellationTokenSource).
 */
class CancellationTokenSource {
public:
    CancellationTokenSource();

    /**
     * @brief Returns a @c CancellationToken linked to this source.
     * @return Associated @c CancellationToken.
     */
    CancellationToken token() const noexcept;

    /**
     * @brief Requests cancellation immediately and notifies all registered callbacks.
     */
    void cancel();

    /**
     * @brief Schedules automatic cancellation after the specified duration.
     * @param[in] delay Time interval after which cancellation is triggered.
     */
    void cancel_after(std::chrono::milliseconds delay);

private:
    std::shared_ptr<detail::CancellationState> state_;
};

/**
 * @struct CancelableSleepAwaiter
 * @brief Awaitable timer object supporting cooperative cancellation via @c CancellationToken.
 */
struct CancelableSleepAwaiter {
    std::chrono::milliseconds duration;
    CancellationToken token;
    std::shared_ptr<std::atomic<bool>> resumed = std::make_shared<std::atomic<bool>>(false);

    bool await_ready() const;
    void await_suspend(std::coroutine_handle<> h) const;
    void await_resume() const;
};

/**
 * @brief Suspends the current coroutine asynchronously with cancellation support.
 * @param[in] duration Time interval to wait before resuming.
 * @param[in] token Cancellation token aborting the sleep early if triggered.
 * @return Awaitable timer object throwing @c OperationCanceledException if canceled.
 */
CancelableSleepAwaiter async_sleep(std::chrono::milliseconds duration, CancellationToken token);

} // namespace aw::tpl
