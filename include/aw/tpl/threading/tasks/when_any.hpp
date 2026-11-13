#pragma once

#include <aw/tpl/threading/tasks/task.hpp>
#include <aw/tpl/threading/tasks/detail/when_any_awaiter.hpp>
#include <aw/tpl/threading/tasks/detail/when_any_state.hpp>
#include <aw/tpl/threading/tasks/detail/when_any_watcher.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace aw::tpl::threading::tasks {

/**
 * @brief Asynchronously awaits until any one of the tasks in the vector completes
 *        (equivalent to C# @c Task.WhenAny<T>(IEnumerable<Task<T>>)).
 *
 * Like in .NET the result is a @c Task<Task<T>>: awaiting it yields the *winning task itself*, which
 * you then await a second time to unwrap its value (or rethrow its failure):
 * @code
 * Task<std::string> completed = co_await when_any(task1, task2);
 * std::string result = co_await completed;   // value, or throws if the winner failed
 * @endcode
 * A failed or canceled task also counts as completed; the outer task itself never reports that failure.
 * The winner's result is not consumed by @c when_any. Losers are not canceled (cancel them yourself
 * through a shared @c CancellationTokenSource).
 *
 * @tparam T Result type of the tasks in the vector (may be @c void).
 * @param[in] tasks Non-empty vector of running tasks.
 * @return Task yielding the first completed @c Task<T> (the very same task, so it compares equal to the
 *         original: @c completed == tasks[i]).
 * @throws std::invalid_argument (through the returned task) If @p tasks is empty.
 */
template <typename T>
Task<Task<T>> when_any(std::vector<Task<T>> tasks) {
    if (tasks.empty()) {
        throw std::invalid_argument("when_any requires at least one task.");
    }

    auto state = std::make_shared<detail::WhenAnyState>();
    for (std::size_t i = 0; i < tasks.size(); ++i) {
        detail::spawn_when_any_watcher(tasks[i].completion(), i, state);
    }

    std::size_t winner = co_await detail::WhenAnyAwaiter{state};
    co_return tasks[winner];
}

/**
 * @brief Asynchronously awaits until any of the provided tasks completes
 *        (equivalent to C# @c Task.WhenAny(t1, t2, ...)).
 *
 * - All tasks share one result type @c T: returns @c Task<Task<T>> (C# @c WhenAny<T>), so the winner's
 *   value is available with a second @c co_await.
 * - Mixed result types: returns @c Task<Task<void>> (C# @c WhenAny(params Task[]) returning @c Task<Task>).
 *   The winner is the same task viewed as @c Task<void>: compare it with your originals using @c ==
 *   (@c if (winner == slow) ...), and await your own typed handle to get the value. Awaiting the
 *   winner itself only waits/rethrows.
 *
 * @tparam Ts Result types of the provided tasks.
 * @param[in] tasks Running tasks to race against each other.
 * @return @c Task<Task<T>> if all @p Ts are the same type @c T, otherwise @c Task<Task<void>>.
 */
template <typename... Ts>
    requires(sizeof...(Ts) > 0)
auto when_any(Task<Ts>... tasks) {
    using First = std::tuple_element_t<0, std::tuple<Ts...>>;
    if constexpr ((std::is_same_v<Ts, First> && ...)) {
        std::vector<Task<First>> vec;
        vec.reserve(sizeof...(Ts));
        (vec.push_back(std::move(tasks)), ...);
        return when_any(std::move(vec));
    } else {
        // Different result types: view every task as the non-generic Task<void> (shared state).
        std::vector<Task<void>> vec;
        vec.reserve(sizeof...(Ts));
        (vec.push_back(Task<void>(tasks)), ...);
        return when_any(std::move(vec));
    }
}

} // namespace aw::tpl::threading::tasks
