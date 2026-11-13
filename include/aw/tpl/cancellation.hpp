#pragma once

#include <aw/tpl/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <coroutine>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
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

/**
 * @class TimeoutException
 * @brief Thrown when an operation does not finish in time (equivalent to C# @c System.TimeoutException).
 *        Typical use: catch @c OperationCanceledException raised by a @c CancellationTokenSource with a
 *        timeout and rethrow it as @c TimeoutException.
 */
class TimeoutException : public std::runtime_error {
public:
    TimeoutException();
    explicit TimeoutException(const std::string& message);
};

namespace detail {

/**
 * @struct CancellationState
 * @brief Shared thread-safe state backing a @c CancellationTokenSource and its tokens.
 */
struct CancellationState {
    std::atomic<bool> canceled{false};
    std::mutex mtx;
    std::vector<std::pair<std::uint64_t, std::function<void()>>> callbacks;
    std::uint64_t next_id = 1;

    /**
     * @brief Transitions the state to canceled and invokes all registered callbacks once.
     *        Every callback runs even if one throws; the first exception is rethrown afterwards.
     */
    void cancel();

    /**
     * @brief Registers a callback to run upon cancellation, or executes it immediately if already canceled.
     * @param[in] cb Callable invoked on cancellation.
     * @return Registration id (> 0), or 0 if the callback was executed immediately.
     */
    std::uint64_t register_callback(std::function<void()> cb);

    /**
     * @brief Removes a previously registered callback.
     * @return True if the callback was still registered.
     */
    bool unregister_callback(std::uint64_t id);

    /**
     * @brief Number of callbacks currently registered (diagnostic).
     */
    std::size_t callback_count();
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
    [[nodiscard]] static CancellationToken none() noexcept;

    /**
     * @brief Checks whether cancellation has been requested.
     * @return True if canceled; false otherwise.
     */
    [[nodiscard]] bool is_cancellation_requested() const noexcept;

    /**
     * @brief Throws @c OperationCanceledException if cancellation was requested.
     * @throws OperationCanceledException When token is canceled.
     */
    void throw_if_cancellation_requested() const;

    /**
     * @brief Registers a callback invoked when cancellation is triggered.
     *        If the token is already canceled the callback runs immediately on the calling thread.
     * @param[in] cb Callable to invoke upon cancellation.
     * @return Registration id (> 0), or 0 if the callback ran immediately or the token cannot be canceled.
     */
    std::uint64_t register_callback(std::function<void()> cb) const;

    /**
     * @brief Removes a callback registered with @c register_callback.
     * @param[in] id Id returned by @c register_callback.
     * @return True if the callback was still registered.
     */
    [[nodiscard]] bool unregister_callback(std::uint64_t id) const;

    /**
     * @brief Number of callbacks currently registered on this token's source (diagnostic).
     */
    [[nodiscard]] std::size_t registered_callback_count() const;

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
     * @brief Creates a source that cancels itself after @p delay
     *        (equivalent to C# @c new CancellationTokenSource(TimeSpan)).
     *        Same as default construction followed by @c cancel_after(delay).
     * @param[in] delay Time interval after which cancellation is triggered.
     */
    explicit CancellationTokenSource(std::chrono::milliseconds delay);

    /**
     * @brief Returns a @c CancellationToken linked to this source.
     * @return Associated @c CancellationToken.
     */
    [[nodiscard]] CancellationToken token() const noexcept;

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
[[nodiscard]] CancelableSleepAwaiter async_sleep(std::chrono::milliseconds duration, CancellationToken token);

} // namespace aw::tpl
