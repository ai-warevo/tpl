#pragma once

/**
 * @file task.hpp
 * @brief @c Task<T> (C# @c System.Threading.Tasks.Task<TResult>) together with the non-generic @c Task<void>.
 */

#include <aw/tpl/threading/tasks/task_fwd.hpp>
#include <aw/tpl/threading/tasks/task_void.hpp>
#include <aw/tpl/threading/tasks/task_already_moved_exception.hpp>
#include <aw/tpl/threading/tasks/detail/completion_awaiter.hpp>
#include <aw/tpl/threading/tasks/detail/is_truly_copyable.hpp>
#include <aw/tpl/threading/tasks/detail/promise.hpp>
#include <aw/tpl/threading/tasks/detail/run_async.hpp>
#include <aw/tpl/threading/tasks/detail/task_state.hpp>

#include <concepts>
#include <coroutine>
#include <exception>
#include <memory>
#include <type_traits>
#include <utility>

namespace aw::tpl::threading::tasks {

/**
 * @class Task
 * @brief Eager, reference-counted C++20 coroutine task modeled after C# @c Task<T>.
 * @tparam T Result type produced by the coroutine. @c Task<void> (the default) is specialized in task_void.hpp.
 */
template <typename T>
class [[nodiscard]] Task {
public:
    using promise_type = detail::Promise<T>;

    Task() noexcept = default;

    /**
     * @brief Constructs a Task referencing the given shared state.
     * @param[in] state Shared state created by the promise.
     */
    explicit Task(std::shared_ptr<detail::TaskState<T>> state) noexcept
        : state_(std::move(state)) {}

    Task(const Task&) = default;
    Task& operator=(const Task&) = default;
    Task(Task&&) noexcept = default;
    Task& operator=(Task&&) noexcept = default;
    ~Task() = default;

    /**
     * @brief Identity comparison (equivalent to C# reference equality of tasks).
     *        True if both handles refer to the same underlying task, also across @c Task<T> and
     *        @c Task<void> views of it. Typical use: @c if (co_await when_any(a, b) == a).
     */
    template <typename U>
    bool operator==(const Task<U>& other) const noexcept {
        return static_cast<const detail::TaskState<void>*>(state_.get()) ==
               static_cast<const detail::TaskState<void>*>(other.state_.get());
    }

    /**
     * @brief Checks whether the task has already finished without suspending.
     * @return True if result is already available.
     */
    bool await_ready() const { return !state_ || state_->is_ready(); }

    /**
     * @brief Suspends the caller until this task finishes using C++20 symmetric transfer,
     *        or immediately resumes @p awaiting if the task completed concurrently.
     * @param[in] awaiting Handle of the coroutine executing @c co_await.
     * @return @p awaiting if already ready; @c std::noop_coroutine() if suspended.
     * @note Not noexcept: attaching the continuation locks a mutex and may allocate.
     *       Only reached when @c await_ready() returned false, i.e. the task has a state.
     */
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) {
        return state_->suspend_or_resume(awaiting);
    }

    /**
     * @brief Returns the task's result or rethrows any captured exception (called by @c co_await).
     * @throws std::logic_error If the task has no shared state (default-constructed or moved-from).
     * @throws TaskAlreadyMovedException If a move-only result was already extracted.
     * @throws Any exception thrown inside the coroutine body.
     */
    T await_resume() const {
        require_state();
        if (state_->exception) {
            std::rethrow_exception(state_->exception);
        }
        if constexpr (detail::is_truly_copyable_v<T>) {
            return *state_->value;
        } else {
            if (!state_->try_take()) {
                throw TaskAlreadyMovedException{};
            }
            return std::move(*state_->value);
        }
    }

    /**
     * @brief Awaitable that only waits for completion: it neither rethrows the task's exception nor
     *        extracts (moves out) the result. Used by combinators such as @c when_any that must not
     *        consume a move-only result on behalf of the user.
     */
    [[nodiscard]] detail::CompletionAwaiter completion() const noexcept { return detail::CompletionAwaiter{state_}; }

    /**
     * @brief Synchronously blocks until completion and returns the computed value
     *        (equivalent to C# @c Task<T>.Result).
     * @throws std::logic_error If the task has no state, or when called on a pool worker for an unfinished task.
     * @throws TaskAlreadyMovedException If a move-only result was already taken.
     * @throws Any exception thrown inside the coroutine body.
     */
    T result() const {
        require_state();
        state_->wait_blocking();
        return await_resume();
    }

    /**
     * @brief Offloads a callable (including capturing lambdas) onto the thread pool safely
     *        (equivalent to C# @c Task.Run). The callable is stored by value inside the coroutine frame.
     * @return @c Task<std::invoke_result_t<Func>> representing the asynchronous execution.
     */
    template <typename Func>
        requires std::invocable<Func>
    static auto run(Func fn) -> Task<std::invoke_result_t<Func>> {
        return detail::run_async(std::move(fn));
    }

private:
    template <typename>
    friend class Task;

    void require_state() const {
        if (!state_) {
            detail::throw_task_has_no_state();
        }
    }

    std::shared_ptr<detail::TaskState<T>> state_;
};

namespace detail {

template <typename T>
Task<T> Promise<T>::get_return_object() noexcept {
    return Task<T>{this->state};
}

} // namespace detail

} // namespace aw::tpl::threading::tasks
