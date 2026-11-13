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
     * @brief Awaiter run at final suspension. It hands ownership of the coroutine frame to the shared
     *        state (which destroys it when the LAST reference to the state is dropped), releases
     *        waiters, and resumes the first awaiting coroutine through symmetric transfer.
     *
     * The frame is deliberately NOT destroyed here. The creating thread's ramp function may still be
     * touching the frame (GCC 15 keeps a non-atomic counter in it that is updated after the handle was
     * scheduled), and the ramp's returned @c Task keeps the state alive until the ramp has returned.
     * Coroutine locals are already destroyed at this point; only by-value parameters live longer.
     */
    struct FinalAwaiter {
        std::shared_ptr<TaskState<T>> st;

        bool await_ready() const noexcept { return false; }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
            auto local = std::move(st); // the promise's own reference moved out in final_suspend()
            local->frame = h;           // published to other owners by finish() / the refcount release
            return local->finish();     // `local` is released on return; the frame dies with the last one
        }

        void await_resume() const noexcept {}
    };

    InitialScheduleAwaiter initial_suspend() noexcept { return {}; }
    FinalAwaiter final_suspend() noexcept {
        // Move (not copy): the promise lives inside the frame, which the state owns after this point,
        // so keeping a reference here would form a state <-> frame ownership cycle.
        return FinalAwaiter{std::move(state)};
    }

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
