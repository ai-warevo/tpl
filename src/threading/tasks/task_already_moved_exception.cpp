#include <aw/tpl/threading/tasks/task_already_moved_exception.hpp>

namespace aw::tpl::threading::tasks {

TaskAlreadyMovedException::TaskAlreadyMovedException()
    : std::logic_error("Task move-only value has already been moved out via co_await or result().") {}

} // namespace aw::tpl::threading::tasks
