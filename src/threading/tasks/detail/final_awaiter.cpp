#include <aw/tpl/threading/tasks/detail/final_awaiter.hpp>

#include <utility>

namespace aw::tpl::threading::tasks::detail {

std::coroutine_handle<> FinalAwaiter::await_suspend(std::coroutine_handle<> h) noexcept {
    auto local = std::move(st); // the promise's own reference moved out in final_suspend()
    local->frame = h;           // published to other owners by finish() / the refcount release
    return local->finish();     // `local` is released on return; the frame dies with the last one
}

} // namespace aw::tpl::threading::tasks::detail
