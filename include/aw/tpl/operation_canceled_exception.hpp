#pragma once

#include <stdexcept>

namespace aw::tpl {

/**
 * @class OperationCanceledException
 * @brief Exception thrown when an asynchronous operation is canceled via a @c CancellationToken
 *        (equivalent to C# @c System.OperationCanceledException).
 */
class OperationCanceledException : public std::runtime_error {
public:
    /// Creates the exception with the .NET default message ("Operation was canceled.").
    OperationCanceledException();
};

} // namespace aw::tpl
