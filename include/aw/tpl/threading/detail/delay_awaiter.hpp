#pragma once

#include <chrono>
#include <coroutine>

namespace aw::tpl::threading::detail {

/**
 * @struct DelayAwaiter
 * @brief Awaitable timer bound to the active or global @c ThreadPool (returned by @c async_sleep).
 */
struct DelayAwaiter {
    std::chrono::milliseconds duration;

    bool await_ready() const noexcept { return duration.count() <= 0; }
    void await_suspend(std::coroutine_handle<> h) const;
    void await_resume() const noexcept {}
};

} // namespace aw::tpl::threading::detail
