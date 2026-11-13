#pragma once

#include <stdexcept>
#include <string>

namespace aw::tpl {

/**
 * @class TimeoutException
 * @brief Thrown when an operation does not finish in time (equivalent to C# @c System.TimeoutException).
 *        Typical use: catch @c OperationCanceledException raised by a @c CancellationTokenSource with a
 *        timeout and rethrow it as @c TimeoutException.
 */
class TimeoutException : public std::runtime_error {
public:
    /// Creates the exception with the .NET default message ("The operation timed out.").
    TimeoutException();

    /**
     * @brief Creates the exception with a custom message.
     * @param[in] message Text returned by @c what().
     */
    explicit TimeoutException(const std::string& message);
};

} // namespace aw::tpl
