#pragma once

#include <aw/tpl/task.hpp>

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
        void unhandled_exception() noexcept {}
    };
};

template <typename T>
WhenAnyWatcher spawn_when_any_watcher(Task<T> task, size_t index, std::shared_ptr<WhenAnyState> state) {
    try {
        if constexpr (std::is_void_v<T>) {
            co_await task;
        } else {
            (void)co_await task;
        }
    } catch (...) {
        // A task completing with an exception still counts as completed in WhenAny
    }

    if (!state->completed.exchange(true, std::memory_order_acq_rel)) {
        state->winner_index = index;
        std::coroutine_handle<> h = nullptr;
        {
            std::lock_guard lock(state->mtx);
            h = state->waiter;
        }
        if (h) {
            schedule_on_pool(h);
        }
    }
}

struct WhenAnyAwaiter {
    std::shared_ptr<WhenAnyState> state;

    bool await_ready() const noexcept {
        return state->completed.load(std::memory_order_acquire);
    }

    bool await_suspend(std::coroutine_handle<> h) noexcept {
        std::lock_guard lock(state->mtx);
        if (state->completed.load(std::memory_order_acquire)) {
            return false;
        }
        state->waiter = h;
        return true;
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
Task<Task<T>> when_any(std::vector<Task<T>> tasks) {
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
Task<size_t> when_any(Task<Ts>... tasks) {
    auto state = std::make_shared<detail::WhenAnyState>();

    size_t index = 0;
    (detail::spawn_when_any_watcher(std::move(tasks), index++, state), ...);

    co_return co_await detail::WhenAnyAwaiter{state};
}

} // namespace aw::tpl
