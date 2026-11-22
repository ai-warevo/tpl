#include <catch2/catch_test_macros.hpp>
#include <aw/tpl/threading/tasks/task.hpp>
#include <aw/tpl/threading/tasks/task_group.hpp>

#include <atomic>
#include <semaphore>
#include <stdexcept>
#include <thread>

using namespace aw::tpl;
using namespace aw::tpl::threading;
using namespace aw::tpl::threading::tasks;

TEST_CASE("TaskGroup starts empty and wait returns immediately", "[task_group]") {
    TaskGroup group;
    REQUIRE(group.pending() == 0);
    group.wait();
}

TEST_CASE("TaskGroup add/done track the counter", "[task_group]") {
    TaskGroup group;
    group.add(2);
    REQUIRE(group.pending() == 2);
    group.done();
    REQUIRE(group.pending() == 1);
    group.done();
    REQUIRE(group.pending() == 0);
}

TEST_CASE("TaskGroup done without add throws", "[task_group]") {
    TaskGroup group;
    REQUIRE_THROWS_AS(group.done(), std::logic_error);
}

TEST_CASE("TaskGroup wait blocks until done", "[task_group]") {
    TaskGroup group;
    group.add();
    std::atomic<bool> released{false};
    std::thread waiter([&] {
        group.wait();
        released = true;
    });
    group.done();
    waiter.join();
    REQUIRE(released);
}

TEST_CASE("TaskGroup wait on a pool worker with pending work throws", "[task_group]") {
    TaskGroup group;
    group.add();
    auto probe = [](TaskGroup& g) -> Task<bool> {
        try {
            g.wait();
        } catch (const std::logic_error&) {
            co_return true;
        }
        co_return false;
    };
    REQUIRE(probe(group).result());
    group.done();
}

TEST_CASE("TaskGroup wait_async resumes when the counter reaches zero", "[task_group]") {
    TaskGroup group;
    group.add(2);
    auto waiter = [](TaskGroup& g) -> Task<int> {
        co_await g.wait_async();
        co_return 7;
    };
    Task<int> t = waiter(group);
    group.done();
    group.done();
    REQUIRE(t.result() == 7);
}

TEST_CASE("TaskGroup wait_async is ready when nothing is pending", "[task_group]") {
    TaskGroup group;
    auto waiter = [](TaskGroup& g) -> Task<int> {
        co_await g.wait_async();
        co_return 1;
    };
    REQUIRE(waiter(group).result() == 1);
}

TEST_CASE("TaskGroup wait_async handles a race with done", "[task_group]") {
    // Many iterations to exercise the window between await_ready() and await_suspend().
    for (int i = 0; i < 200; ++i) {
        TaskGroup group;
        group.add();
        auto waiter = [](TaskGroup& g) -> Task<void> { co_await g.wait_async(); };
        Task<void> t = waiter(group);
        std::thread finisher([&] { group.done(); });
        t.wait();
        finisher.join();
    }
}

TEST_CASE("TaskGroup releases several async waiters", "[task_group]") {
    TaskGroup group;
    group.add();
    auto waiter = [](TaskGroup& g) -> Task<void> { co_await g.wait_async(); };
    Task<void> a = waiter(group);
    Task<void> b = waiter(group);
    Task<void> c = waiter(group);
    group.done();
    a.wait();
    b.wait();
    c.wait();
}

TEST_CASE("TaskGroup add with count greater than 1", "[task_group]") {
    TaskGroup group;
    group.add(5);
    REQUIRE(group.pending() == 5);
    group.done();
    REQUIRE(group.pending() == 4);
    group.done();
    REQUIRE(group.pending() == 3);
    group.done();
    REQUIRE(group.pending() == 2);
    group.done();
    group.done();
    REQUIRE(group.pending() == 0);
}

TEST_CASE("TaskGroup multiple done calls throw correctly", "[task_group]") {
    TaskGroup group;
    REQUIRE_THROWS_AS(group.done(), std::logic_error);
    
    group.add();
    group.done();
    REQUIRE_THROWS_AS(group.done(), std::logic_error);
}
