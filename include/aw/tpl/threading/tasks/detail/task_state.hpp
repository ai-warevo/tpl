#pragma once

#include <aw/tpl/threading/tasks/detail/task_state_base.hpp>

#include <atomic>
#include <optional>

namespace aw::tpl::threading::tasks::detail {

/**
 * @struct TaskState
 * @brief Shared state storing a return value of type @p T.
 * @tparam T Result type produced by the coroutine.
 *
 * Every @c TaskState<T> derives from @c TaskState<void> (the completion-only state), mirroring how
 * C# @c Task<T> derives from the non-generic @c Task. This lets a @c Task<T> be viewed as a
 * @c Task<void> that shares the very same state (see @c Task<void>'s converting constructor).
 */
template <typename T>
struct TaskState;

/**
 * @struct TaskState<void>
 * @brief Shared state specialization for void-returning tasks (also the common base of all states).
 */
template <>
struct TaskState<void> : TaskStateBase {};

template <typename T>
struct TaskState : TaskState<void> {
    std::optional<T> value;
    std::atomic<bool> value_taken{false};

    /**
     * @brief One-shot claim used when a move-only result is extracted.
     * @return True for the first caller only.
     */
    bool try_take() noexcept { return !value_taken.exchange(true, std::memory_order_acq_rel); }
};

} // namespace aw::tpl::threading::tasks::detail
