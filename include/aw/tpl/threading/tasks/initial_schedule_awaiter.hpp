#pragma once

#include <aw/tpl/thread_pool.hpp>

#include <coroutine>

namespace aw::tpl::threading::tasks {

/**
 * @struct InitialScheduleAwaiter
 * @brief Immediately schedules a newly created coroutine onto the active or global @c ThreadPool.
 */
struct InitialScheduleAwaiter {
    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) const {
        ::aw::tpl::detail::schedule_on_pool(h);
    }

    void await_resume() const noexcept {}
};

} // namespace aw::tpl::threading::tasks
