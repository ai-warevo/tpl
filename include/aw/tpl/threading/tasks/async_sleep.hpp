#pragma once

#include <aw/tpl/threading/cancellation_token.hpp>
#include <aw/tpl/threading/detail/delay_awaiter.hpp>
#include <aw/tpl/threading/tasks/detail/cancelable_sleep_awaiter.hpp>

#include <chrono>

namespace aw::tpl::threading::tasks {

/**
 * @brief Suspends the current coroutine asynchronously for the specified duration (like C# @c Task.Delay).
 * @param[in] duration Time interval to wait before resuming the coroutine.
 * @return Awaitable timer object bound to the active or global @c ThreadPool.
 */
[[nodiscard]] threading::detail::DelayAwaiter async_sleep(std::chrono::milliseconds duration) noexcept;

/**
 * @brief Suspends the current coroutine asynchronously with cancellation support
 *        (like C# @c Task.Delay(TimeSpan, CancellationToken)).
 * @param[in] duration Time interval to wait before resuming.
 * @param[in] token Cancellation token aborting the sleep early if triggered.
 * @return Awaitable timer object throwing @c OperationCanceledException if canceled.
 */
[[nodiscard]] detail::CancelableSleepAwaiter async_sleep(std::chrono::milliseconds duration, CancellationToken token);

} // namespace aw::tpl::threading::tasks
