#pragma once

#include <aw/tpl/threading/tasks/detail/when_any_state.hpp>

#include <coroutine>
#include <cstddef>
#include <memory>

namespace aw::tpl::threading::tasks::detail {

/**
 * @struct WhenAnyAwaiter
 * @brief Suspends the @c when_any coroutine until one watcher has claimed the win; yields the winner's index.
 */
struct WhenAnyAwaiter {
    std::shared_ptr<WhenAnyState> state;

    bool await_ready() const noexcept;
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> h);
    std::size_t await_resume() const noexcept;
};

} // namespace aw::tpl::threading::tasks::detail
