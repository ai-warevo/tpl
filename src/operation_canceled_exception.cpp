#include <aw/tpl/operation_canceled_exception.hpp>

namespace aw::tpl {

OperationCanceledException::OperationCanceledException()
    : std::runtime_error("Operation was canceled.") {}

} // namespace aw::tpl
