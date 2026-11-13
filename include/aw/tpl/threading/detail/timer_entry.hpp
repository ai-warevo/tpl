#pragma once

#include <chrono>
#include <coroutine>

namespace aw::tpl::threading::detail {

/**
 * @struct TimerEntry
 * @brief A scheduled timer event of the @c ThreadPool, ordered by expiration time.
 */
struct TimerEntry {
    std::chrono::steady_clock::time_point wake_time; ///< Timestamp when the coroutine should resume.
    std::coroutine_handle<> handle;                  ///< Suspended coroutine handle.

    bool operator>(const TimerEntry& other) const noexcept {
        return wake_time > other.wake_time;
    }
};

} // namespace aw::tpl::threading::detail
