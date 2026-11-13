#pragma once

#include <aw/tpl/thread_pool.hpp>
#include <aw/tpl/threading/tasks/promise.hpp>
#include <aw/tpl/threading/tasks/task_exceptions.hpp>
#include <aw/tpl/threading/tasks/task_state.hpp>

#include <array>
#include <concepts>
#include <coroutine>
#include <deque>
#include <exception>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace aw::tpl::threading::tasks {

namespace task_detail {

/**
 * @brief True if T can really be copied. Unlike std::is_copy_constructible_v this looks inside
 *        standard containers, so e.g. std::vector<std::unique_ptr<int>> is reported as not copyable.
 */
template <typename T>
struct is_truly_copyable : std::is_copy_constructible<T> {};

template <typename E, typename A>
struct is_truly_copyable<std::vector<E, A>> : std::bool_constant<is_truly_copyable<E>::value> {};
template <typename E, typename A>
struct is_truly_copyable<std::deque<E, A>> : std::bool_constant<is_truly_copyable<E>::value> {};
template <typename E, typename A>
struct is_truly_copyable<std::list<E, A>> : std::bool_constant<is_truly_copyable<E>::value> {};
template <typename E, std::size_t N>
struct is_truly_copyable<std::array<E, N>> : std::bool_constant<is_truly_copyable<E>::value> {};
template <typename E>
struct is_truly_copyable<std::optional<E>> : std::bool_constant<is_truly_copyable<E>::value> {};
template <typename A, typename B>
struct is_truly_copyable<std::pair<A, B>> : std::bool_constant<is_truly_copyable<A>::value && is_truly_copyable<B>::value> {};
template <typename... Es>
struct is_truly_copyable<std::tuple<Es...>> : std::bool_constant<(is_truly_copyable<Es>::value && ...)> {};
template <typename... Es>
struct is_truly_copyable<std::variant<Es...>> : std::bool_constant<(is_truly_copyable<Es>::value && ...)> {};

template <typename T>
inline constexpr bool is_truly_copyable_v = is_truly_copyable<T>::value;

} // namespace task_detail

/**
 * @class Task
 * @brief Eager, reference-counted C++20 coroutine task modeled after C# @c Task<T>.
 * @tparam T Result type produced by the coroutine (defaults to @c void).
 */
template <typename T>
class [[nodiscard]] Task {
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
     * @note Not noexcept: attaching the continuation locks a mutex and may allocate.
     */
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) {
        if (!state_) {
            return awaiting; // await_resume() will throw std::logic_error
        }
        if (!state_->try_attach_continuation(awaiting)) {
            return awaiting;
        }
        return std::noop_coroutine();
    }

    /**
     * @brief Returns the task's result or rethrows any captured exception (called by @c co_await).
     * @return Result of type @p T (or void).
     * @throws std::logic_error If the task has no shared state (default-constructed or moved-from).
     * @throws TaskAlreadyMovedException If a move-only result was already extracted.
     * @throws Any exception thrown inside the coroutine body.
     */
    T await_resume() const {
        require_state();
        if (state_->exception) {
            std::rethrow_exception(state_->exception);
        }
        if constexpr (!std::is_void_v<T>) {
            if constexpr (task_detail::is_truly_copyable_v<T>) {
                return *state_->value;
            } else {
                if (!state_->try_take()) {
                    throw TaskAlreadyMovedException{};
                }
                return std::move(*state_->value);
            }
        }
    }

    /**
     * @brief Awaitable that only waits for completion: it neither rethrows the task's exception nor
     *        extracts (moves out) the result. Used by combinators such as @c when_any that must not
     *        consume a move-only result on behalf of the user.
     */
    auto completion() const noexcept {
        struct CompletionAwaiter {
            std::shared_ptr<TaskState<T>> state;
            bool await_ready() const { return !state || state->is_ready(); }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) {
                if (!state->try_attach_continuation(awaiting)) {
                    return awaiting;
                }
                return std::noop_coroutine();
            }
            void await_resume() const noexcept {}
        };
        return CompletionAwaiter{state_};
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
     * @throws std::logic_error If the task has no state, or when called on a pool worker for an unfinished task.
     * @throws TaskAlreadyMovedException If a move-only result was already taken.
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
    void require_state() const {
        if (!state_) {
            throw std::logic_error("Task has no shared state (default-constructed or moved-from).");
        }
    }

    void wait_until_ready() const {
        require_state();
        if (!state_->is_ready()) {
            // A pool worker blocking here can starve the pool (guaranteed deadlock on a single worker).
            if (::aw::tpl::detail::current_pool != nullptr) {
                throw std::logic_error("Blocking wait on a thread-pool worker would deadlock; use co_await instead.");
            }
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
