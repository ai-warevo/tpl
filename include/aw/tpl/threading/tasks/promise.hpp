#pragma once

#include <aw/tpl/threading/tasks/initial_schedule_awaiter.hpp>
#include <aw/tpl/threading/tasks/task_state.hpp>

#include <coroutine>
#include <exception>
#include <memory>
#include <type_traits>
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

    /**
     * @brief Awaiter run at final suspension. It destroys the coroutine frame (and therefore every
     *        local variable) BEFORE waiters are released, then resumes the first awaiting coroutine
     *        through symmetric transfer. The awaiter owns its own reference to the shared state,
     *        so the state outlives the destroyed frame.
     */
    struct FinalAwaiter {
        std::shared_ptr<TaskState<T>> st;

        bool await_ready() const noexcept { return false; }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
            auto local = std::move(st);
            h.destroy();
            return local->finish();
        }

        void await_resume() const noexcept {}
    };

    InitialScheduleAwaiter initial_suspend() noexcept { return {}; }
    FinalAwaiter final_suspend() noexcept { return FinalAwaiter{state}; }

    /**
     * @brief Stores an unhandled exception into the shared state; completion happens in final_suspend.
     */
    void unhandled_exception() noexcept {
        state->exception = std::current_exception();
    }
};

template <typename T>
struct Promise : PromiseBase<T> {
    Task<T> get_return_object() noexcept;

    static_assert(!std::is_reference_v<T>, "Task<T&> is not supported; return a value or a pointer instead.");

    void return_value(T v) {
        this->state->value.emplace(std::move(v));
    }
};

template <>
struct Promise<void> : PromiseBase<void> {
    Task<void> get_return_object() noexcept;

    void return_void() {}
};

} // namespace aw::tpl::threading::tasks
