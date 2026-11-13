#pragma once

#include <stdexcept>

namespace aw::tpl::threading::tasks {

/**
 * @class TaskAlreadyMovedException
 * @brief Thrown when attempting to extract a move-only result from a @c Task<T> more than once.
 */
class TaskAlreadyMovedException : public std::logic_error {
public:
    TaskAlreadyMovedException();
};

} // namespace aw::tpl::threading::tasks
