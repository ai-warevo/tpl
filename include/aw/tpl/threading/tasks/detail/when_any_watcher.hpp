#pragma once

#include <aw/tpl/threading/tasks/detail/completion_awaiter.hpp>
#include <aw/tpl/threading/tasks/detail/when_any_state.hpp>

#include <cstddef>
#include <memory>

namespace aw::tpl::threading::tasks::detail {

/**
 * @brief Starts a detached coroutine that waits for @p completion (a failed task still counts as
 *        completed; a move-only result stays available for the caller) and then tries to win the
 *        race recorded in @p state under @p index.
 */
void spawn_when_any_watcher(CompletionAwaiter completion, std::size_t index, std::shared_ptr<WhenAnyState> state);

} // namespace aw::tpl::threading::tasks::detail
