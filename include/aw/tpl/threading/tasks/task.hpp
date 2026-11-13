#pragma once

#include <aw/tpl/threading/tasks/promise.hpp>
#include <aw/tpl/threading/tasks/task_exceptions.hpp>
#include <aw/tpl/threading/tasks/task_state.hpp>

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
 * @tparam T Result type produced by the coroutine (defaults to @c void).
 */
template <typename T>
class Task {
public:
    using promise_type = Promise<T>;

    Task() noexcept = default;

    /**
     * @brief Constructs a Task referencing the given shared state.
     * @param[in] state Shared state created by the promise.
     */
    explicit Task(std::shared_ptr<TaskState<T>> state) noexcept
        : state_(std::move(state)) {}

    Task(const Task&) = default;
    Task& operator=(const Task&) = default;
    Task(Task&&) noexcept = default;
    Task& operator=(Task&&) noexcept = default;
    ~Task() = default;

    /**
     * @brief Checks whether the task has already finished without suspending.
     * @return True if result is already available.
     */
    bool await_ready() const {
        return !state_ || state_->is_ready();
    }

    /**
     * @brief Suspends the caller until this task finishes using C++20 symmetric transfer,
     *        or immediately resumes @p awaiting if the task completed concurrently.
     * @param[in] awaiting Handle of the coroutine executing @c co_await.
     * @return @p awaiting if already ready; @c std::noop_coroutine() if suspended.
     */
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept {
        if (!state_->try_attach_continuation(awaiting)) {
            return awaiting;
        }
        return std::noop_coroutine();
    }

    /**
     * @brief Returns the task's result or rethrows any captured exception (called by @c co_await).
     * @return Result of type @p T (or void).
     * @throws Any exception thrown inside the coroutine body.
     */
    T await_resume() const {
        if (state_->exception) {
            std::rethrow_exception(state_->exception);
        }
        if constexpr (!std::is_void_v<T>) {
            if constexpr (std::is_copy_constructible_v<T>) {
                return state_->value;
            } else {
                return std::move(state_->value);
            }
        }
    }

    /**
     * @brief Synchronously blocks the calling thread until the void task completes
     *        (equivalent to C# @c Task.Wait()). Enabled only for @c Task<void>.
     * @throws Any exception thrown inside the coroutine body.
     */
    void wait() const requires (std::is_void_v<T>) {
        wait_until_ready();
        await_resume();
    }

    /**
     * @brief Synchronously blocks until completion and returns the computed value
     *        (equivalent to C# @c Task<T>.Result). Enabled only for non-void @c Task<T>.
     * @return Result of type @p T.
     * @throws Any exception thrown inside the coroutine body.
     */
    T result() const requires (!std::is_void_v<T>) {
        wait_until_ready();
        return await_resume();
    }

    /**
     * @brief Offloads a callable (including capturing lambdas) onto the thread pool safely
     *        (equivalent to C# @c Task.Run). Captures the callable by value inside the coroutine frame
     *        to prevent dangling-lambda lifetime issues.
     * @tparam Func Callable type (lambda, function pointer, functor).
     * @param[in] fn Callable to execute on a worker thread.
     * @return @c Task<std::invoke_result_t<Func>> representing the asynchronous execution.
     */
    template <typename Func>
        requires std::invocable<Func>
    static auto run(Func fn) -> Task<std::invoke_result_t<Func>> {
        using Ret = std::invoke_result_t<Func>;
        if constexpr (std::is_void_v<Ret>) {
            fn();
            co_return;
        } else {
            co_return fn();
        }
    }

private:
    /**
     * @brief Blocks the calling thread on the completion semaphore until the task finishes.
     */
    void wait_until_ready() const {
        if (!state_->is_ready()) {
            state_->done_sem.acquire();
            state_->done_sem.release();
        }
    }

    std::shared_ptr<TaskState<T>> state_;
};

template <typename T>
Task<T> Promise<T>::get_return_object() noexcept {
    return Task<T>{this->state};
}

inline Task<void> Promise<void>::get_return_object() noexcept {
    return Task<void>{this->state};
}

} // namespace aw::tpl::threading::tasks
