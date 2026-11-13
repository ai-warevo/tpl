#pragma once

#include <aw/tpl/threading/tasks/task_fwd.hpp>

#include <concepts>
#include <type_traits>

namespace aw::tpl::threading::tasks::detail {

/**
 * @brief Coroutine behind @c Task::run: executes @p fn on a pool worker. The callable is a by-value
 *        coroutine parameter, i.e. it lives inside the coroutine frame (no dangling-lambda issues).
 *        Requires the complete @c Task<R> at the point of instantiation (include task.hpp).
 */
template <typename Func>
    requires std::invocable<Func>
auto run_async(Func fn) -> Task<std::invoke_result_t<Func>> {
    using Ret = std::invoke_result_t<Func>;
    if constexpr (std::is_void_v<Ret>) {
        fn();
        co_return;
    } else {
        co_return fn();
    }
}

} // namespace aw::tpl::threading::tasks::detail
