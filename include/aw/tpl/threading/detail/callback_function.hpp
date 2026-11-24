#ifndef AW_TPL_THREADING_DETAIL_CALLBACK_FUNCTION_HPP
#define AW_TPL_THREADING_DETAIL_CALLBACK_FUNCTION_HPP

#if __has_include(<version>)
#include <version>
#endif

#include <functional>

namespace aw::tpl::threading::detail {

/**
 * @brief Type-erased callback using @c std::move_only_function when available, @c std::function as fallback.
 * 
 * This conditional typedef enables zero-copy callback storage on C++23 compilers,
 * with graceful fallback for older standards.
 * 
 * @tparam Signature Function signature for the callback.
 */
template <typename Signature>
#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
using CallbackFunction = std::move_only_function<Signature>;
#else
using CallbackFunction = std::function<Signature>;
#endif

} // namespace aw::tpl::threading::detail

#endif  // AW_TPL_THREADING_DETAIL_CALLBACK_FUNCTION_HPP
