#ifndef AW_TPL_THREADING_TASKS_DETAIL_PROMISE_VOID_HPP
#define AW_TPL_THREADING_TASKS_DETAIL_PROMISE_VOID_HPP

#include <aw/tpl/threading/tasks/task_fwd.hpp>
#include <aw/tpl/threading/tasks/detail/promise.hpp>

namespace aw::tpl::threading::tasks::detail {

/**
 * @struct Promise<void>
 * @brief Promise type of the non-generic @c Task. @c get_return_object is defined in promise_void.cpp.
 */
template <>
struct Promise<void> : PromiseBase<void> {
    /**
     * @brief Creates the @c Task<void> handle sharing this promise's state.
     * @return A new @c Task<void> holding a reference to this promise's shared state.
     */
    Task<void> get_return_object() noexcept;

    void return_void() {}
};

} // namespace aw::tpl::threading::tasks::detail

#endif  // AW_TPL_THREADING_TASKS_DETAIL_PROMISE_VOID_HPP
