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
 * @brief Asynchronously awaits multiple heterogeneous tasks without blocking any worker thread
 *        (equivalent to C# @c Task.WhenAll(t1, t2, ...)).
 * @tparam Ts Result types of the provided tasks.
 * @param[in] tasks Running tasks to await concurrently.
 * @return Task producing a @c std::tuple of results (@c std::monostate is used for @c Task<void>).
 */
template <typename... Ts>
Task<std::tuple<detail::WhenAllElementT<Ts>...>> when_all(Task<Ts>... tasks) {
    auto await_one = []<typename T>(Task<T>& t) -> Task<detail::WhenAllElementT<T>> {
        if constexpr (std::is_void_v<T>) {
            co_await t;
            co_return std::monostate{};
        } else {
            co_return co_await t;
        }
    };

    co_return std::make_tuple((co_await await_one(tasks))...);
}

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

} // namespace aw::tpl
