#pragma once

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <mutex>

namespace aw::tpl::threading::tasks::detail {

/**
 * @struct WhenAnyState
 * @brief State shared between the awaiting @c when_any coroutine and its per-task watchers.
 */
struct WhenAnyState {
    std::atomic<bool> completed{false};
    std::mutex mtx;
    std::coroutine_handle<> waiter = nullptr;
    std::size_t winner_index = 0;

    /**
     * @brief Called by a watcher whose task finished. The first caller wins: it records its index,
     *        publishes @c completed and schedules the waiting coroutine (if any) on the pool.
     * @return True if this call won the race.
     */
    bool try_complete(std::size_t index);
};

} // namespace aw::tpl::threading::tasks::detail
