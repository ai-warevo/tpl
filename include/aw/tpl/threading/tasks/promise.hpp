#pragma once

#include <aw/tpl/threading/tasks/initial_schedule_awaiter.hpp>
#include <aw/tpl/threading/tasks/task_state.hpp>

#include <coroutine>
#include <exception>
#include <memory>
#include <utility>

namespace aw::tpl::threading::tasks {

template <typename T = void>
class Task;

/**
 * @struct PromiseBase
 * @brief Base promise type for eager C#-style coroutines.
 * @tparam T Result type of the associated @c Task.
 */
template <typename T>
struct PromiseBase {
    std::shared_ptr<TaskState<T>> state = std::make_shared<TaskState<T>>();

    InitialScheduleAwaiter initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    /**
     * @brief Stores an unhandled exception into the shared state and notifies waiters.
     */
    void unhandled_exception() noexcept {
        state->exception = std::current_exception();
        state->complete();
    }
};

template <typename T>
struct Promise : PromiseBase<T> {
    Task<T> get_return_object() noexcept;

    template <typename U>
    void return_value(U&& v) {
        this->state->value = std::forward<U>(v);
        this->state->complete();
    }
};

template <>
struct Promise<void> : PromiseBase<void> {
    Task<void> get_return_object() noexcept;

    void return_void() {
        this->state->complete();
    }
};

} // namespace aw::tpl::threading::tasks
