#pragma once

#include <aw/tpl/task.hpp>

#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace aw::tpl {

namespace detail {

template <typename T>
struct WhenAllElement {
    using type = T;
};

template <>
struct WhenAllElement<void> {
    using type = std::monostate;
};

template <typename T>
using WhenAllElementT = typename WhenAllElement<T>::type;

} // namespace detail

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
            for (auto& t : list) {
                co_await t;
            }
            co_return;
        }(std::move(tasks));
    } else {
        return [](std::vector<Task<T>> list) -> Task<std::vector<T>> {
            std::vector<T> results;
            results.reserve(list.size());
            for (auto& t : list) {
                results.push_back(co_await t);
            }
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
        auto await_one = []<typename T>(Task<T>& t) -> Task<detail::WhenAllElementT<T>> {
            if constexpr (std::is_void_v<T>) {
                co_await t;
                co_return std::monostate{};
            } else {
                co_return co_await t;
            }
        };

        return [](auto helper, Task<Ts>... list) -> Task<std::tuple<detail::WhenAllElementT<Ts>...>> {
            co_return std::make_tuple((co_await helper(list))...);
        }(await_one, std::move(tasks)...);
    }
}

} // namespace aw::tpl
