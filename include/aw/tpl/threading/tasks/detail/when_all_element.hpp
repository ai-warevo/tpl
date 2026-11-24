#ifndef AW_TPL_THREADING_TASKS_DETAIL_WHEN_ALL_ELEMENT_HPP
#define AW_TPL_THREADING_TASKS_DETAIL_WHEN_ALL_ELEMENT_HPP

#include <variant>

namespace aw::tpl::threading::tasks::detail {

/**
 * @brief Element type stored per child task by @c when_all.
 * 
 * Maps each child result type @c T directly (e.g. @c int → @c int),
 * with the special case that @c void children are stored as @c std::monostate
 * to allow uniform tuple-like access to all results.
 * 
 * @tparam T The child task's result type.
 */
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

#endif  // AW_TPL_THREADING_TASKS_DETAIL_WHEN_ALL_ELEMENT_HPP
