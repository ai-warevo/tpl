#pragma once

#include <aw/tpl/threading/tasks/detail/final_awaiter.hpp>
#include <aw/tpl/threading/tasks/detail/initial_schedule_awaiter.hpp>
#include <aw/tpl/threading/tasks/detail/task_state.hpp>

#include <exception>
#include <memory>
#include <utility>

namespace aw::tpl::threading::tasks::detail {

/**
 * @struct PromiseBase
 * @brief Base promise type for eager C#-style coroutines.
 * @tparam T Result type of the associated @c Task.
 */
template <typename T>
struct PromiseBase {
    std::shared_ptr<TaskState<T>> state = std::make_shared<TaskState<T>>();

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

} // namespace aw::tpl::threading::tasks::detail
