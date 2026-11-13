#pragma once

// Non-generic Task (C# System.Threading.Tasks.Task). Include <aw/tpl/threading/tasks/task.hpp> instead:
// it provides both this class and the generic Task<T>. Everything that does not depend on a template
// parameter is implemented in src/threading/tasks/task_void.cpp.

#include <aw/tpl/threading/tasks/task_fwd.hpp>
#include <aw/tpl/threading/tasks/detail/completion_awaiter.hpp>
#include <aw/tpl/threading/tasks/detail/promise_void.hpp>
#include <aw/tpl/threading/tasks/detail/run_async.hpp>
#include <aw/tpl/threading/tasks/detail/task_state.hpp>

#include <concepts>
#include <coroutine>
#include <memory>
#include <type_traits>
#include <utility>

namespace aw::tpl::threading::tasks {

/**
 * @class Task<void>
 * @brief Eager, reference-counted C++20 coroutine task without a result (C# @c Task).
 */
template <>
class [[nodiscard]] Task<void> {
public:
    using promise_type = detail::Promise<void>;

    Task() noexcept = default;

    /**
     * @brief Constructs a Task referencing the given shared state.
     * @param[in] state Shared state created by the promise.
     */
    explicit Task(std::shared_ptr<detail::TaskState<void>> state) noexcept;

    Task(const Task&) = default;
    Task& operator=(const Task&) = default;
    Task(Task&&) noexcept = default;
    Task& operator=(Task&&) noexcept = default;
    ~Task() = default;

    /**
     * @brief Views a @c Task<U> as the non-generic @c Task<void> (like C# upcasting @c Task<T> to @c Task).
     *        Both handles share one state, so the result is the *same* task: it compares equal to the
     *        source and completes (or fails) together with it. Awaiting it observes completion and
     *        rethrows a failure, but yields no value and never consumes a move-only result.
     * @tparam U Result type of the source task (non-void).
     * @param[in] other Source task.
     */
    template <typename U>
        requires (!std::is_void_v<U>)
    Task(const Task<U>& other) noexcept  // NOLINT(google-explicit-constructor): implicit like C# upcast
        : state_(other.state_) {}

    /**
     * @brief Identity comparison (equivalent to C# reference equality of tasks).
     *        True if both handles refer to the same underlying task, also across @c Task<T> and
     *        @c Task<void> views of it.
     */
    template <typename U>
    bool operator==(const Task<U>& other) const noexcept {
        return static_cast<const detail::TaskState<void>*>(state_.get()) ==
               static_cast<const detail::TaskState<void>*>(other.state_.get());
    }

    /// Checks whether the task has already finished without suspending.
    bool await_ready() const;

    /**
     * @brief Suspends the caller until this task finishes using C++20 symmetric transfer,
     *        or immediately resumes @p awaiting if the task completed concurrently.
     * @note Not noexcept: attaching the continuation locks a mutex and may allocate.
     */
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting);

    /**
     * @brief Rethrows any captured exception (called by @c co_await).
     * @throws std::logic_error If the task has no shared state (default-constructed or moved-from).
     * @throws Any exception thrown inside the coroutine body.
     */
    void await_resume() const;

    /**
     * @brief Awaitable that only waits for completion without rethrowing the task's exception
     *        (used by combinators such as @c when_any).
     */
    detail::CompletionAwaiter completion() const noexcept;

    /**
     * @brief Synchronously blocks the calling thread until the task completes
     *        (equivalent to C# @c Task.Wait()).
     * @throws Any exception thrown inside the coroutine body.
     */
    void wait() const;

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

    std::shared_ptr<detail::TaskState<void>> state_;
};

} // namespace aw::tpl::threading::tasks
