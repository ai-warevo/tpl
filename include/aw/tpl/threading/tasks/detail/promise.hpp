#pragma once

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

    Task<T> get_return_object() noexcept;

    void return_value(T v) {
        this->state->value.emplace(std::move(v));
    }
};

} // namespace aw::tpl::threading::tasks::detail
