#include <aw/tpl/threading/tasks/detail/initial_schedule_awaiter.hpp>

#include <aw/tpl/threading/detail/pool_scheduling.hpp>

namespace aw::tpl::threading::tasks::detail {

void InitialScheduleAwaiter::await_suspend(std::coroutine_handle<> h) const {
    threading::detail::schedule_on_pool(h);
}

} // namespace aw::tpl::threading::tasks::detail
