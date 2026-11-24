// co_spawn: fire-and-forget tasks, error reporting, and draining with a TaskGroup.
#include <aw/tpl/tpl.hpp>

#include <atomic>
#include <exception>
#include <print>
#include <stdexcept>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading::tasks;

Task<void> work(std::atomic<int>& counter) {
    co_await async_sleep(10ms);
    ++counter;
}

Task<void> failing() {
    co_await async_sleep(5ms);
    throw std::runtime_error("something went wrong");
}

int main() {
    std::atomic<int> counter{0};
    TaskGroup group;

    // Track several detached tasks and drain them.
    for (int i = 0; i < 10; ++i) {
        co_spawn(group, work(counter));
    }

    // Failures are reported to the handler instead of being dropped silently.
    co_spawn(group, failing(), [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            std::print("spawned task failed: {}\n", ex.what());
        }
    });

    group.wait();
    std::print("completed: {}\n", counter.load());
    return 0;
}
