#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace aw::tpl::threading {

class CancellationTokenSource;

namespace detail {
struct CancellationState;
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

} // namespace aw::tpl::threading
