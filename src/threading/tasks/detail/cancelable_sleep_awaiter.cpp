#include <aw/tpl/threading/tasks/detail/cancelable_sleep_awaiter.hpp>

#include <aw/tpl/threading/detail/delay_awaiter.hpp>
#include <aw/tpl/threading/detail/detached.hpp>
#include <aw/tpl/threading/detail/pool_scheduling.hpp>

#include <atomic>
#include <cstdint>
#include <memory>

namespace aw::tpl::threading::tasks::detail {

bool CancelableSleepAwaiter::await_ready() const {
    token.throw_if_cancellation_requested();
    return duration.count() <= 0;
}

void CancelableSleepAwaiter::await_suspend(std::coroutine_handle<> h) const {
    // Copy everything needed into locals first: once the handle can be resumed by another thread
    // the coroutine frame (which owns this awaiter) may be destroyed, so `this` must not be used
    // after the timer/callback have been armed. A fresh flag per suspension also makes the same
    // awaiter object safely awaitable more than once.
    struct Control {
        std::atomic<bool> resumed{false};
        std::atomic<std::uint64_t> callback_id{0};
    };

    const auto d = duration;
    CancellationToken local_token = token;
    // Resume on the pool the coroutine was running on, not on whatever thread fires the cancel.
    ThreadPool* origin_pool = threading::detail::current_pool();
    auto ctl = std::make_shared<Control>();

    // Cancellation path: the callback has already been consumed by cancel(), nothing to unregister.
    auto try_resume = [ctl, h, origin_pool]() {
        if (!ctl->resumed.exchange(true, std::memory_order_acq_rel)) {
            threading::detail::schedule_on_pool(origin_pool, h);
        }
    };

    // Arm the timer first, then the cancellation callback (which may run inline if already canceled).
    [](std::chrono::milliseconds delay, std::shared_ptr<Control> c, CancellationToken tok, std::coroutine_handle<> handle, ThreadPool* pool) -> threading::detail::Detached {
        co_await threading::detail::DelayAwaiter{delay};
        if (!c->resumed.exchange(true, std::memory_order_acq_rel)) {
            // Timer won: drop the now useless cancel callback so a long-lived token does not grow.
            // Done BEFORE resuming the coroutine, using only copies owned by this bridge frame.
            (void)tok.unregister_callback(c->callback_id.load());
            threading::detail::schedule_on_pool(pool, handle);
        }
        co_return;
    }(d, ctl, local_token, h, origin_pool).start();

    const auto id = local_token.register_callback(try_resume);
    ctl->callback_id.store(id);
    // The timer may have won before the id was published; then it could not unregister, so do it here.
    if (id != 0 && ctl->resumed.load()) {
        (void)local_token.unregister_callback(id);
    }
}

void CancelableSleepAwaiter::await_resume() const {
    token.throw_if_cancellation_requested();
}

} // namespace aw::tpl::threading::tasks::detail
