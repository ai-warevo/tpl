#include <aw/tpl/threading/tasks/detail/promise_void.hpp>

#include <aw/tpl/threading/tasks/task_void.hpp>

namespace aw::tpl::threading::tasks::detail {

Task<void> Promise<void>::get_return_object() noexcept {
    return Task<void>{this->state};
}

} // namespace aw::tpl::threading::tasks::detail
