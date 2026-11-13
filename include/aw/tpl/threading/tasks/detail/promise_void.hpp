#pragma once

#include <aw/tpl/threading/tasks/task_fwd.hpp>
#include <aw/tpl/threading/tasks/detail/promise.hpp>

namespace aw::tpl::threading::tasks::detail {

/**
 * @struct Promise<void>
 * @brief Promise type of the non-generic @c Task. @c get_return_object is defined in promise_void.cpp.
 */
template <>
struct Promise<void> : PromiseBase<void> {
    Task<void> get_return_object() noexcept;

    void return_void() {}
};

} // namespace aw::tpl::threading::tasks::detail
