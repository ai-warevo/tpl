#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/threading/tasks/spawn.hpp>
#include <aw/tpl/threading/tasks/task.hpp>
#include <aw/tpl/threading/tasks/task_group.hpp>

#include <atomic>
#include <exception>
#include <memory>
#include <semaphore>
#include <stdexcept>
#include <string>

using namespace aw::tpl;
using namespace aw::tpl::threading;
using namespace aw::tpl::threading::tasks;

namespace {

Task<void> signal_task(std::binary_semaphore& sem) {
    sem.release();
    co_return;
}

Task<void> failing_task() {
    throw std::runtime_error("boom");
    co_return;
}

Task<int> failing_int_task() {
    throw std::runtime_error("boom-int");
    co_return 1;
}

Task<int> value_task() {
    co_return 5;
}

} // namespace

TEST_CASE("co_spawn runs a detached task", "[spawn]") {
    std::binary_semaphore sem{0};
    co_spawn(signal_task(sem));
    sem.acquire();
    SUCCEED();
}

TEST_CASE("co_spawn discards failures without a handler", "[spawn]") {
    TaskGroup group;
    co_spawn(group, failing_task());
    group.wait();
    REQUIRE(group.pending() == 0);
}

TEST_CASE("co_spawn reports a failure to the handler", "[spawn]") {
    std::binary_semaphore sem{0};
    std::string message;
    co_spawn(failing_task(), [&](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            message = ex.what();
        }
        sem.release();
    });
    sem.acquire();
    REQUIRE(message == "boom");
}

TEST_CASE("co_spawn does not call the handler on success", "[spawn]") {
    TaskGroup group;
    std::atomic<int> calls{0};
    co_spawn(group, value_task(), [&](std::exception_ptr) { ++calls; });
    group.wait();
    REQUIRE(calls == 0);
}

TEST_CASE("co_spawn handles Task<T> failures", "[spawn]") {
    TaskGroup group;
    std::string message;
    co_spawn(group, failing_int_task(), [&](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            message = ex.what();
        }
    });
    group.wait();
    REQUIRE(message == "boom-int");
}

TEST_CASE("co_spawn reports a default-constructed task through the handler", "[spawn]") {
    TaskGroup group;
    std::atomic<bool> got_error{false};
    co_spawn(group, Task<void>{}, [&](std::exception_ptr ep) { got_error = static_cast<bool>(ep); });
    group.wait();
    REQUIRE(got_error);
}

TEST_CASE("co_spawn swallows an exception thrown by the handler", "[spawn]") {
    TaskGroup group;
    co_spawn(group, failing_task(), [](std::exception_ptr) { throw std::runtime_error("handler failed"); });
    group.wait();
    REQUIRE(group.pending() == 0);
}

TEST_CASE("co_spawn with a group counts in-flight tasks and drains", "[spawn]") {
    TaskGroup group;
    std::atomic<int> finished{0};
    constexpr int kTasks = 50;
    auto work = [](std::atomic<int>& n) -> Task<void> {
        ++n;
        co_return;
    };
    for (int i = 0; i < kTasks; ++i) {
        co_spawn(group, work(finished));
    }
    group.wait();
    REQUIRE(finished == kTasks);
}

TEST_CASE("co_spawn group is released after the handler's captures are destroyed", "[spawn]") {
    TaskGroup group;
    auto token = std::make_shared<int>(1);
    std::weak_ptr<int> weak = token;
    co_spawn(group, failing_task(), [token = std::move(token)](std::exception_ptr) {});
    group.wait();
    REQUIRE(weak.expired());
}

TEST_CASE("co_spawn group can be awaited from a coroutine", "[spawn]") {
    TaskGroup group;
    co_spawn(group, value_task());
    auto drain = [](TaskGroup& g) -> Task<void> { co_await g.wait_async(); };
    drain(group).wait();
    REQUIRE(group.pending() == 0);
}

TEST_CASE("co_spawn with group and void task success", "[spawn]") {
    TaskGroup group;
    std::atomic<bool> handler_called{false};
    
    co_spawn(group, []() -> Task<void> { co_return; }(), [&](std::exception_ptr) {
        handler_called = true;
    });
    
    group.wait();
    REQUIRE_FALSE(handler_called);
}
