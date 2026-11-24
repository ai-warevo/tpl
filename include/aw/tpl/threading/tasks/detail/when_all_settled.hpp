#ifndef AW_TPL_THREADING_TASKS_DETAIL_WHEN_ALL_SETTLED_HPP
#define AW_TPL_THREADING_TASKS_DETAIL_WHEN_ALL_SETTLED_HPP

#include <aw/tpl/threading/tasks/detail/is_truly_copyable.hpp>
#include <aw/tpl/threading/tasks/detail/when_all_element.hpp>

#include <exception>
#include <optional>
#include <type_traits>

namespace aw::tpl::threading::tasks::detail {

/**
 * @brief Outcome of one child task: either a value or the exception it failed with.
 */
template <typename T>
struct WhenAllSettled {
    std::optional<WhenAllElementT<T>> value;
    std::exception_ptr exception;
};

/* A settled outcome is only copyable if its payload is (the optional's copy
   constructor lies for containers), so we specialize is_truly_copyable. */
template <typename T>
struct is_truly_copyable<WhenAllSettled<T>> : std::bool_constant<is_truly_copyable<WhenAllElementT<T>>::value> {};

} // namespace aw::tpl::threading::tasks::detail

#endif  // AW_TPL_THREADING_TASKS_DETAIL_WHEN_ALL_SETTLED_HPP
