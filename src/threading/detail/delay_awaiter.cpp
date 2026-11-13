#include <aw/tpl/threading/detail/delay_awaiter.hpp>

#include <aw/tpl/threading/detail/pool_scheduling.hpp>

namespace aw::tpl::threading::detail {

void DelayAwaiter::await_suspend(std::coroutine_handle<> h) const {
    schedule_after_on_pool(nullptr, h, duration);
}

} // namespace aw::tpl::threading::detail
