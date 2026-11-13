#pragma once

#include <aw/tpl/thread_pool.hpp>

#include <coroutine>
#include <exception>
#include <memory>
#include <mutex>
#include <semaphore>
#include <type_traits>
#include <utility>

namespace aw::tpl {

template <typename T = void>
class Task;

namespace detail {

/**
 * @struct InitialScheduleAwaiter
 * @brief Immediately schedules a newly created coroutine onto the active or global @c ThreadPool.
 */
struct InitialScheduleAwaiter {
    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) const {
        schedule_on_pool(h);
    }

    void await_resume() const noexcept {}
};

/**
 * @struct TaskStateBase
 * @brief Shared synchronization state between a running coroutine and its @c Task handle(s).
 */
struct TaskStateBase {
    std::mutex mtx;
    bool ready = false;
    std::coroutine_handle<> continuation = nullptr;
    std::exception_ptr exception = nullptr;
    std::binary_semaphore done_sem{0};

    /**
     * @brief Checks whether the task has completed in a thread-safe manner.
     * @return True if result or exception is ready.
     */
    bool is_ready() {
        std::lock_guard lock(mtx);
        return ready;
    }

    /**
     * @brief Registers an awaiting coroutine if the task is not yet complete.
     * @param[in] awaiting Handle of the caller coroutine.
     * @return True if suspended and @p awaiting will be resumed later; false if already ready.
     */
    bool try_attach_continuation(std::coroutine_handle<> awaiting) {
        std::lock_guard lock(mtx);
        if (ready) {
            return false;
        }
        continuation = awaiting;
        return true;
    }

    /**
     * @brief Marks the task as finished, releases blocking waiters, and resumes any continuation.
     */
    void complete() {
        std::coroutine_handle<> cont = nullptr;
        {
            std::lock_guard lock(mtx);
            ready = true;
            cont = continuation;
        }
        done_sem.release();
        if (cont) {
            cont.resume();
        }
    }
};

/**
 * @struct TaskState
 * @brief Shared state specialization storing a return value of type @p T.
 * @tparam T Result type produced by the coroutine.
 */
template <typename T>
struct TaskState : TaskStateBase {
    T value{};
};

/**
 * @struct TaskState<void>
 * @brief Shared state specialization for void-returning tasks.
 */
template <>
struct TaskState<void> : TaskStateBase {};

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

} // namespace detail

/**
 * @class Task
 * @brief Eager, reference-counted C++20 coroutine task modeled after C# @c Task<T>.
 * @tparam T Result type produced by the coroutine (defaults to @c void).
 */
template <typename T>
class Task {
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

    std::shared_ptr<detail::TaskState<T>> state_;
};

namespace detail {

template <typename T>
Task<T> Promise<T>::get_return_object() noexcept {
    return Task<T>{this->state};
}

inline Task<void> Promise<void>::get_return_object() noexcept {
    return Task<void>{this->state};
}

} // namespace detail

} // namespace aw::tpl
