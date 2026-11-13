#include <aw/tpl/threading/tasks/detail/cancelable_sleep_awaiter.hpp>

#include <aw/tpl/threading/detail/delay_awaiter.hpp>
#include <aw/tpl/threading/detail/detached.hpp>
#include <aw/tpl/threading/detail/pool_scheduling.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

namespace aw::tpl::threading::tasks::detail {

namespace {

/// Whoever flips @c resumed first (timer or cancellation) resumes the coroutine; the loser does nothing.
struct SleepControl {
    std::atomic<bool> resumed{false};
};

/**
 * @brief Timer side of a cancelable sleep. Takes everything by value: it must not touch the awaiter,
 *        whose coroutine frame may already be destroyed once the handle can be resumed elsewhere.
 */
threading::detail::Detached sleep_timer(std::chrono::milliseconds delay, std::shared_ptr<SleepControl> control,
                                        CancellationToken token, std::uint64_t callback_id, std::coroutine_handle<> h,
                                        ThreadPool* pool) {
    co_await threading::detail::DelayAwaiter{delay};
    if (!control->resumed.exchange(true, std::memory_order_acq_rel)) {
        // Drop the now useless cancel callback BEFORE resuming, so a long-lived token does not grow.
        (void)token.unregister_callback(callback_id);
        threading::detail::schedule_on_pool(pool, h);
    }
}

} // namespace

bool CancelableSleepAwaiter::await_ready() const {
    token.throw_if_cancellation_requested();
    return duration.count() <= 0;
}

void CancelableSleepAwaiter::await_suspend(std::coroutine_handle<> h) const {
    // Copy what is needed into locals: after the callback is armed another thread may resume and
    // destroy the coroutine frame that owns this awaiter, so `this` must not be used any more.
    const auto d = duration;
    CancellationToken local_token = token;
    // Resume on the pool the coroutine was running on, not on whatever thread fires the cancel.
    ThreadPool* origin_pool = threading::detail::current_pool();
    auto control = std::make_shared<SleepControl>();

    // The callback is registered first so its id is known before the timer exists. If the token is
    // already canceled it runs inline and the timer later finds `resumed` set.
    const auto id = local_token.register_callback([control, h, origin_pool] {
        if (!control->resumed.exchange(true, std::memory_order_acq_rel)) {
            threading::detail::schedule_on_pool(origin_pool, h);
        }
    });

    sleep_timer(d, std::move(control), std::move(local_token), id, h, origin_pool).start();
}

void CancelableSleepAwaiter::await_resume() const {
    token.throw_if_cancellation_requested();
}

} // namespace aw::tpl::threading::tasks::detail
