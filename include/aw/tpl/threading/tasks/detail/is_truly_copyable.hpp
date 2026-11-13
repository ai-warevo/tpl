#pragma once

#include <array>
#include <cstddef>
#include <deque>
#include <list>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace aw::tpl::threading::tasks::detail {

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

} // namespace aw::tpl::threading::tasks::detail
