#pragma once

#include <aw/tpl/threading/cancellation_token.hpp>

#include <chrono>
#include <memory>

namespace aw::tpl::threading {

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

} // namespace aw::tpl::threading
