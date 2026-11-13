#include <aw/tpl/threading/detail/cancellation_state.hpp>

#include <exception>

namespace aw::tpl::threading::detail {

void CancellationState::cancel() {
    std::vector<std::pair<std::uint64_t, std::function<void()>>> to_run;
    {
        std::lock_guard lock(mtx);
        if (canceled.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        to_run.swap(callbacks);
    }
    // The lock is not held while callbacks run. A throwing callback must not prevent the others
    // from running (they may resume sleeping coroutines), so remember the first error and rethrow it.
    std::exception_ptr first_error;
    for (auto& entry : to_run) {
        if (!entry.second) continue;
        try {
            entry.second();
        } catch (...) {
            if (!first_error) first_error = std::current_exception();
        }
    }
    if (first_error) std::rethrow_exception(first_error);
}

std::uint64_t CancellationState::register_callback(std::function<void()> cb) {
    std::uint64_t id = 0;
    bool run_now = false;
    {
        std::lock_guard lock(mtx);
        if (canceled.load(std::memory_order_acquire)) {
            run_now = true;
        } else {
            id = next_id++;
            callbacks.emplace_back(id, std::move(cb));
        }
    }
    // An exception from an immediately executed callback propagates to the caller.
    if (run_now && cb) {
        cb();
    }
    return id;
}

bool CancellationState::unregister_callback(std::uint64_t id) {
    if (id == 0) return false;
    std::lock_guard lock(mtx);
    for (auto it = callbacks.begin(); it != callbacks.end(); ++it) {
        if (it->first == id) {
            callbacks.erase(it);
            return true;
        }
    }
    return false;
}

std::size_t CancellationState::callback_count() {
    std::lock_guard lock(mtx);
    return callbacks.size();
}

} // namespace aw::tpl::threading::detail
