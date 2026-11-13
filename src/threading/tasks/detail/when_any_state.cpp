#include <aw/tpl/threading/tasks/detail/when_any_state.hpp>

#include <aw/tpl/threading/detail/pool_scheduling.hpp>

namespace aw::tpl::threading::tasks::detail {

bool WhenAnyState::try_complete(std::size_t index) {
    // The winner index is written BEFORE `completed` is published (release). Readers observe
    // `completed` with acquire (await_ready) or under `mtx` (await_suspend), so they always see
    // the matching index. Claiming the win under the lock makes the claim and the waiter lookup atomic.
    std::coroutine_handle<> h = nullptr;
    bool won = false;
    {
        std::lock_guard lock(mtx);
        if (!completed.load(std::memory_order_relaxed)) {
            winner_index = index;
            completed.store(true, std::memory_order_release);
            h = waiter;
            won = true;
        }
    }
    if (won && h) {
        threading::detail::schedule_on_pool(h);
    }
    return won;
}

} // namespace aw::tpl::threading::tasks::detail
