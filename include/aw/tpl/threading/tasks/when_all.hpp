#pragma once

#include <aw/tpl/threading/tasks/task.hpp>
#include <aw/tpl/threading/tasks/detail/when_all_element.hpp>
#include <aw/tpl/threading/tasks/detail/when_all_settled.hpp>

#include <exception>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace aw::tpl::threading::tasks {

/**
 * @brief Asynchronously awaits all tasks in a vector (equivalent to C# @c Task.WhenAll(IEnumerable<Task<T>>)).
 * @tparam T Result type of the tasks in the vector.
 * @param[in] tasks Vector of running tasks.
 * @return Task yielding a @c std::vector<T> of results (or @c Task<void> if @p T is @c void).
 */
template <typename T>
auto when_all(std::vector<Task<T>> tasks) {
    if constexpr (std::is_void_v<T>) {
        return [](std::vector<Task<void>> list) -> Task<void> {
            // Await EVERY child before reporting a failure, so no sibling keeps running unobserved.
            std::exception_ptr first;
            for (auto& t : list) {
                try {
                    co_await t;
                } catch (...) {
                    if (!first) first = std::current_exception();
                }
            }
            if (first) std::rethrow_exception(first);
            co_return;
        }(std::move(tasks));
    } else {
        return [](std::vector<Task<T>> list) -> Task<std::vector<T>> {
            std::vector<T> results;
            results.reserve(list.size());
            std::exception_ptr first;
            for (auto& t : list) {
                try {
                    results.push_back(co_await t);
                } catch (...) {
                    if (!first) first = std::current_exception();
                }
            }
            if (first) std::rethrow_exception(first);
            co_return results;
        }(std::move(tasks));
    }
}

/**
 * @brief Asynchronously awaits multiple tasks without blocking any worker thread
 *        (equivalent to C# @c Task.WhenAll(t1, t2, ...)).
 *        Returns @c Task<void> if all tasks return @c void; otherwise returns @c Task<std::tuple<...>>.
 * @tparam Ts Result types of the provided tasks.
 * @param[in] tasks Running tasks to await concurrently.
 * @return @c Task<void> or @c Task<std::tuple<...>>.
 */
template <typename... Ts>
    requires (sizeof...(Ts) > 0)
auto when_all(Task<Ts>... tasks) {
    if constexpr ((std::is_void_v<Ts> && ...)) {
        std::vector<Task<void>> vec;
        vec.reserve(sizeof...(Ts));
        (vec.push_back(std::move(tasks)), ...);
        return when_all(std::move(vec));
    } else {
        // The helper never throws: it records either the value or the exception, so awaiting one
        // child can never abandon the others.
        auto settle = []<typename T>(Task<T>& t) -> Task<detail::WhenAllSettled<T>> {
            detail::WhenAllSettled<T> outcome;
            try {
                if constexpr (std::is_void_v<T>) {
                    co_await t;
                    outcome.value.emplace();
                } else {
                    outcome.value.emplace(co_await t);
                }
            } catch (...) {
                outcome.exception = std::current_exception();
            }
            co_return outcome;
        };

        return [](auto helper, Task<Ts>... list) -> Task<std::tuple<detail::WhenAllElementT<Ts>...>> {
            // Braced initialisation guarantees left-to-right evaluation, so children are awaited in order.
            std::tuple<detail::WhenAllSettled<Ts>...> outcomes{(co_await helper(list))...};

            std::exception_ptr first;
            std::apply([&first](auto&... o) {
                ((void)(first || (first = o.exception)), ...);
            }, outcomes);
            if (first) std::rethrow_exception(first);

            co_return std::apply([](auto&... o) {
                return std::tuple<detail::WhenAllElementT<Ts>...>(std::move(*o.value)...);
            }, outcomes);
        }(settle, std::move(tasks)...);
    }
}

} // namespace aw::tpl::threading::tasks
