#ifndef AW_TPL_OPERATION_CANCELED_EXCEPTION_HPP
#define AW_TPL_OPERATION_CANCELED_EXCEPTION_HPP

#include <exception>
#include <string>

namespace aw::tpl {

/**
 * @class OperationCanceledException
 * @brief Exception thrown when an asynchronous operation is canceled via a @c CancellationToken
 *        (equivalent to C# @c System.OperationCanceledException).
 */
class OperationCanceledException : public std::exception {
public:
    /**
     * @brief Creates the exception with the .NET default message ("Operation was canceled.").
     */
    OperationCanceledException();

    /**
     * @brief Creates the exception with a custom reason.
     * @param[in] reason Brief description of why the operation was canceled.
     */
    explicit OperationCanceledException(std::string reason);

    /**
     * @brief Returns the exception message.
     * @return Null-terminated C-string containing the exception message.
     */
    [[nodiscard]] const char* what() const noexcept override;

    /**
     * @brief Returns the reason the operation was canceled.
     * @return Reason string (empty if created with the default message).
     */
    [[nodiscard]] const std::string& exception_reason() const noexcept { return reason_; }

private:
    std::string message_;
    std::string reason_;
};

} // namespace aw::tpl

#endif  // AW_TPL_OPERATION_CANCELED_EXCEPTION_HPP
