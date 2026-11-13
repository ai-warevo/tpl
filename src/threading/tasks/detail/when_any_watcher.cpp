#include <aw/tpl/threading/tasks/detail/when_any_watcher.hpp>

#include <aw/tpl/threading/detail/detached.hpp>

#include <utility>

namespace aw::tpl::threading::tasks::detail {

namespace {

threading::detail::Detached watch(CompletionAwaiter completion, std::size_t index, std::shared_ptr<WhenAnyState> state) {
    // Wait for completion only: a failed task still counts as completed, and a move-only result
    // must stay available for the caller (the winner is returned as a Task).
    co_await completion;
    state->try_complete(index);
}

} // namespace

void spawn_when_any_watcher(CompletionAwaiter completion, std::size_t index, std::shared_ptr<WhenAnyState> state) {
    watch(std::move(completion), index, std::move(state)).start();
}

} // namespace aw::tpl::threading::tasks::detail
