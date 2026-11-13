#include <aw/tpl/threading/tasks/async_sleep.hpp>

#include <utility>

namespace aw::tpl::threading::tasks {

threading::detail::DelayAwaiter async_sleep(std::chrono::milliseconds duration) noexcept {
    return threading::detail::DelayAwaiter{duration};
}

detail::CancelableSleepAwaiter async_sleep(std::chrono::milliseconds duration, CancellationToken token) {
    return detail::CancelableSleepAwaiter{duration, std::move(token)};
}

} // namespace aw::tpl::threading::tasks
