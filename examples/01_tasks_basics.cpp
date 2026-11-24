// Task<T> basics: eager start, co_await, blocking wait()/result(), exceptions, move-only results.
#include <aw/tpl/tpl.hpp>

#include <atomic>
#include <format>
#include <memory>
#include <print>
#include <algorithm>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading;
using namespace aw::tpl::threading::tasks;

Task<int> square(int x) {
    co_await async_sleep(50ms);
    co_return x * x;
}

// Uses std::source_location to attach call site info to every log line.
Task<void> log_line(std::string text,
                    std::source_location loc = std::source_location::current()) {
    co_await async_sleep(10ms);
    std::print("  log [{}:{}]: {}\n", loc.file_name(), loc.line(), text);
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
    int a = co_await square(3);
    int b = co_await square(4);
    co_return a + b;
}

// Helper: std::format<bool> prints "true"/"false" but we normalise to lowercase.
inline std::string bool_str(bool v) { return std::format("{}", v ? "true" : "false"); }

// Helper: uses if constexpr to handle std::atomic vs plain types uniformly.
template <typename T>
std::string to_printable(T const& v) {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::atomic<int>>) {
        return std::format("{}", v.load());
    } else {
        return std::format("{}", v);
    }
}

int main() {
    Task<int> t = square(7);
    std::print("square(7) = {}\n", t.result());

    // Task<void> uses wait().
    log_line("hello from a Task<void>").wait();

    // Awaiting inside a coroutine.
    std::print("chain() = {}\n", chain().result());

    // A finished task can be read again (copyable results).
    std::print("square(7) again = {}\n", t.result());
    std::print("await_ready() after completion: {}\n", bool_str(t.await_ready()));

    // Exceptions travel through the Task.
    try {
        (void)always_fails().result();
    } catch (const std::runtime_error& ex) {
        std::print("caught: {}\n", ex.what());
    }

    // Move-only results can be taken exactly once.
    auto p = make_unique_result();
    std::print("unique_ptr value = {}\n", *p.result());
    try {
        (void)p.result();
    } catch (const TaskAlreadyMovedException&) {
        std::print("second take throws: {} (move-only)\n",
                   std::source_location::current().function_name());
    }

    // A default-constructed (or moved-from) Task has no state and throws std::exception.
    Task<int> empty;
    try {
        (void)empty.result();
    } catch (const std::exception& ex) {
        std::print("empty task: {}\n", ex.what());
    }

    std::vector<int> vals = {7, 49, 121};
    std::print("std::for_each:");
    std::ranges::for_each(vals, [](int v) { std::print(" {}", v); });
    std::print("\n");

    std::atomic<int> atomic_val{42};
    int plain_val{99};
    std::print("atomic via to_printable: {}\n", to_printable(atomic_val));
    std::print("plain  via to_printable: {}\n", to_printable(plain_val));

    return 0;
}
