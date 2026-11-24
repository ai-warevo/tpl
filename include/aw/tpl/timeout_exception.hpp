#ifndef AW_TPL_TIMEOUT_EXCEPTION_HPP
#define AW_TPL_TIMEOUT_EXCEPTION_HPP

#include <exception>
#include <string>

namespace aw::tpl {

/**
 * @class TimeoutException
 * @brief Thrown when an operation does not finish in time (equivalent to C# @c System.TimeoutException).
 *        Typical use: catch @c OperationCanceledException raised by a @c CancellationTokenSource with a
 *        timeout and rethrow it as @c TimeoutException.
 */
class TimeoutException : public std::exception {
public:
    /**
     * @brief Creates the exception with the .NET default message ("The operation timed out.").
     */
    TimeoutException();

    /**
     * @brief Creates the exception with a custom message.
     * @param[in] message Text returned by @c what().
     */
    explicit TimeoutException(std::string message);

    /**
     * @brief Creates the exception with a custom message and reason.
     * @param[in] message Text returned by @c what().
     * @param[in] reason Brief description of what operation timed out.
     */
    TimeoutException(std::string message, std::string reason);

    /**
     * @brief Returns the exception message.
     * @return Null-terminated C-string containing the exception message.
     */
    [[nodiscard]] const char* what() const noexcept override;

    /**
     * @brief Returns the reason the operation timed out.
     * @return Reason string (empty if not provided).
     */
    [[nodiscard]] const std::string& exception_reason() const noexcept { return reason_; }

private:
    std::string message_;
    std::string reason_;
};

} // namespace aw::tpl

#endif  // AW_TPL_TIMEOUT_EXCEPTION_HPP
