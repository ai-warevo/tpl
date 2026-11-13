// Task<T> basics: eager start, co_await, blocking wait()/result(), exceptions, move-only results.
#include <aw/tpl/tpl.hpp>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading;
using namespace aw::tpl::threading::tasks;

Task<int> square(int x) {
    co_await async_sleep(50ms);
    co_return x* x;
}

Task<void> log_line(std::string text) {
    co_await async_sleep(10ms);
    std::cout << "  log: " << text << "\n";
}

Task<int> always_fails() {
    co_await async_sleep(10ms);
    throw std::runtime_error("something went wrong");
    co_return 0;
}

Task<std::unique_ptr<int>> make_unique_result() {
    co_await async_sleep(10ms);
    co_return std::make_unique<int>(42);
}

Task<int> chain() {
    // co_await inside a coroutine never blocks a pool thread.
    int a = co_await square(3);
    int b = co_await square(4);
    co_return a + b;
}

int main() {
    // Tasks start eagerly on the thread pool; result() blocks the (non-pool) calling thread.
    Task<int> t = square(7);
    std::cout << "square(7) = " << t.result() << "\n";

    // Task<void> uses wait().
    log_line("hello from a Task<void>").wait();

    // Awaiting inside a coroutine.
    std::cout << "chain() = " << chain().result() << "\n";

    // A finished task can be read again (copyable results).
    std::cout << "square(7) again = " << t.result() << "\n";
    std::cout << "await_ready() after completion: " << std::boolalpha << t.await_ready() << "\n";

    // Exceptions travel through the Task.
    try {
        (void)always_fails().result();
    } catch (const std::runtime_error& ex) {
        std::cout << "caught: " << ex.what() << "\n";
    }

    // Move-only results can be taken exactly once.
    auto p = make_unique_result();
    std::cout << "unique_ptr value = " << *p.result() << "\n";
    try {
        (void)p.result();
    } catch (const TaskAlreadyMovedException& ex) {
        std::cout << "second take throws: " << ex.what() << "\n";
    }

    // A default-constructed (or moved-from) Task has no state and throws std::logic_error.
    Task<int> empty;
    try {
        (void)empty.result();
    } catch (const std::logic_error& ex) {
        std::cout << "empty task: " << ex.what() << "\n";
    }
    return 0;
}
