#pragma once

#include <aw/tpl/threading/tasks/task.hpp>

#include <atomic>
#include <coroutine>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace aw::tpl {

namespace detail {

struct WhenAnyState {
    std::atomic<bool> completed{false};
    std::mutex mtx;
    std::coroutine_handle<> waiter = nullptr;
    size_t winner_index = 0;
};

struct WhenAnyWatcher {
    struct promise_type {
        WhenAnyWatcher get_return_object() noexcept { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept {} // LCOV_EXCL_LINE
    };
};

template <typename T>
WhenAnyWatcher spawn_when_any_watcher(threading::tasks::Task<T> task, size_t index, std::shared_ptr<WhenAnyState> state) {
    // Wait for completion only: a failed task still counts as completed, and a move-only result
    // must stay available for the caller (the winner is returned as a Task).
    co_await task.completion();

    // The winner index is written BEFORE `completed` is published (release). Readers observe
    // `completed` with acquire (await_ready) or under `mtx` (await_suspend), so they always see
    // the matching index. Claiming the win under the lock makes the claim and the waiter lookup atomic.
    std::coroutine_handle<> h = nullptr;
    bool won = false;
    {
        std::lock_guard lock(state->mtx);
        if (!state->completed.load(std::memory_order_relaxed)) {
            state->winner_index = index;
            state->completed.store(true, std::memory_order_release);
            h = state->waiter;
            won = true;
        }
    }
    if (won && h) {
        schedule_on_pool(h);
    }
}

struct WhenAnyAwaiter {
    std::shared_ptr<WhenAnyState> state;

    bool await_ready() const noexcept {
        return state->completed.load(std::memory_order_acquire);
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
        std::lock_guard lock(state->mtx);
        // Reading winner_index later is race free: it is written before completed is set (see watcher).
        if (state->completed.load(std::memory_order_acquire)) {
            return h;
        }
        state->waiter = h;
        return std::noop_coroutine();
    }

    size_t await_resume() const noexcept {
        return state->winner_index;
    }
};

} // namespace detail

/**
 * @brief Asynchronously awaits until any one of the tasks in the vector completes
 *        (equivalent to C# @c Task.WhenAny(IEnumerable<Task<T>>)).
 * @tparam T Result type of the tasks in the vector.
 * @param[in] tasks Non-empty vector of running tasks.
 * @return Task yielding the first completed @c Task<T> instance.
 * @throws std::invalid_argument If @p tasks is empty.
 */
template <typename T>
threading::tasks::Task<threading::tasks::Task<T>> when_any(std::vector<threading::tasks::Task<T>> tasks) {
    if (tasks.empty()) {
        throw std::invalid_argument("when_any requires at least one task.");
    }

    auto state = std::make_shared<detail::WhenAnyState>();
    for (size_t i = 0; i < tasks.size(); ++i) {
        detail::spawn_when_any_watcher(tasks[i], i, state);
    }

    size_t winner = co_await detail::WhenAnyAwaiter{state};
    co_return tasks[winner];
}

/**
 * @brief Asynchronously awaits until any of the provided heterogeneous tasks completes
 *        (equivalent to C# @c Task.WhenAny(t1, t2, ...)).
 * @tparam Ts Result types of the provided tasks.
 * @param[in] tasks Running tasks to race against each other.
 * @return Task yielding the zero-based index of the first completed task.
 */
template <typename... Ts>
    requires (sizeof...(Ts) > 0)
threading::tasks::Task<size_t> when_any(threading::tasks::Task<Ts>... tasks) {
    auto state = std::make_shared<detail::WhenAnyState>();

    // Move tasks into a tuple to allow indexed access via index_sequence.
    auto task_tuple = std::make_tuple(std::move(tasks)...);

    // Use a template lambda to expand the index sequence and spawn watchers deterministically.
    []<std::size_t... Is>(std::tuple<threading::tasks::Task<Ts>...>& t, std::index_sequence<Is...>, auto& s) {
        (detail::spawn_when_any_watcher(std::move(std::get<Is>(t)), Is, s), ...);
    }(task_tuple, std::index_sequence_for<Ts...>{}, state);
    co_return co_await detail::WhenAnyAwaiter{state};
}

} // namespace aw::tpl
