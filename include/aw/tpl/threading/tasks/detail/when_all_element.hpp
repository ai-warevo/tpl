#pragma once

#include <variant>

namespace aw::tpl::threading::tasks::detail {

/// Element type stored per child by @c when_all: the result type, with @c void mapped to @c std::monostate.
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

} // namespace aw::tpl::threading::tasks::detail
