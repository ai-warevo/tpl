#include <aw/tpl/threading/tasks/detail/task_state_base.hpp>

#include <aw/tpl/threading/detail/pool_scheduling.hpp>

#include <stdexcept>

namespace aw::tpl::threading::tasks::detail {

void throw_task_has_no_state() {
    throw std::logic_error("Task has no shared state (default-constructed or moved-from).");
}

TaskStateBase::~TaskStateBase() {
    if (frame) {
        frame.destroy();
    }
}

bool TaskStateBase::is_ready() {
    std::lock_guard lock(mtx);
    return ready;
}

bool TaskStateBase::try_attach_continuation(std::coroutine_handle<> awaiting) {
    std::lock_guard lock(mtx);
    if (ready) {
        return false;
    }
    continuations.push_back(awaiting);
    return true;
}

std::coroutine_handle<> TaskStateBase::suspend_or_resume(std::coroutine_handle<> awaiting) {
    if (!try_attach_continuation(awaiting)) {
        return awaiting;
    }
    return std::noop_coroutine();
}

std::coroutine_handle<> TaskStateBase::finish() {
    std::vector<std::coroutine_handle<>> to_resume;
    {
        std::lock_guard lock(mtx);
        ready = true;
        to_resume = std::move(continuations);
        continuations.clear();
    }
    done_sem.release();

    if (to_resume.empty()) {
        return std::noop_coroutine();
    }

    // The first continuation is resumed by symmetric transfer, the rest are scheduled on the pool.
    std::coroutine_handle<> first = to_resume.front();
    for (std::size_t i = 1; i < to_resume.size(); ++i) {
        threading::detail::schedule_on_pool(to_resume[i]);
    }
    return first;
}

void TaskStateBase::complete() {
    // Use finish() to perform cleanup and get the handle for symmetric transfer.
    std::coroutine_handle<> first = finish();
    first.resume();
}

void TaskStateBase::wait_blocking() {
    if (!is_ready()) {
        // A pool worker blocking here can starve the pool (guaranteed deadlock on a single worker).
        if (threading::detail::current_pool() != nullptr) {
            throw std::logic_error("Blocking wait on a thread-pool worker would deadlock; use co_await instead.");
        }
        done_sem.acquire();
        done_sem.release();
    }
}

} // namespace aw::tpl::threading::tasks::detail
