#include <aw/tpl/timeout_exception.hpp>

namespace aw::tpl {

TimeoutException::TimeoutException()
    : std::runtime_error("The operation timed out.") {}

TimeoutException::TimeoutException(const std::string& message)
    : std::runtime_error(message) {}

} // namespace aw::tpl
