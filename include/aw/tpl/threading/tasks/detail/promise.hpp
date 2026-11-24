#ifndef AW_TPL_THREADING_TASKS_DETAIL_PROMISE_HPP
#define AW_TPL_THREADING_TASKS_DETAIL_PROMISE_HPP

#include <aw/tpl/threading/tasks/task_fwd.hpp>
#include <aw/tpl/threading/tasks/detail/promise_base.hpp>

#include <type_traits>
#include <utility>

namespace aw::tpl::threading::tasks::detail {

/**
 * @struct Promise
 * @brief Promise type of @c Task<T>. @c get_return_object is defined in task.hpp (needs the complete Task).
 */
template <typename T>
struct Promise : PromiseBase<T> {
    static_assert(!std::is_reference_v<T>, "Task<T&> is not supported; return a value or a pointer instead.");

    /**
     * @brief Creates the @c Task<T> handle sharing this promise's state.
     * @return A new @c Task holding a reference to this promise's shared state.
     */
    Task<T> get_return_object() noexcept;

    /**
     * @brief Stores the coroutine's result in the shared state.
     * 
     * Called by the compiler when a coroutine reaches a @c co_return statement.
     * Thread-safe; guarded by the state's internal mutex.
     * 
     * @param[in] v Result value; moved into the shared state.
     */
    void return_value(T v) { this->state->value.emplace(std::move(v)); }
};

} // namespace aw::tpl::threading::tasks::detail

#endif  // AW_TPL_THREADING_TASKS_DETAIL_PROMISE_HPP
