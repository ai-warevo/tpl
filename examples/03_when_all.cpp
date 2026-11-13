// when_all: wait for several tasks (variadic with mixed types, or a vector of tasks).
#include <aw/tpl/tpl.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading::tasks;

Task<int> number(int v, std::chrono::milliseconds delay) {
    co_await async_sleep(delay);
    co_return v;
}

Task<std::string> text(std::string s, std::chrono::milliseconds delay) {
    co_await async_sleep(delay);
    co_return s;
}

Task<void> nothing(std::chrono::milliseconds delay) {
    co_await async_sleep(delay);
}

Task<int> failing() {
    co_await async_sleep(10ms);
    throw std::runtime_error("child failed");
    co_return 0;
}

Task<void> app() {
    // 1. Heterogeneous: a tuple of results; Task<void> children yield std::monostate.
    auto [n, s, unit] = co_await when_all(number(1, 120ms), text("two", 60ms), nothing(30ms));
    std::cout << "tuple: " << n << ", " << s << ", monostate? "
              << std::boolalpha << std::is_same_v<decltype(unit), std::monostate> << "\n";

    // 2. Only Task<void> children: the result is a Task<void>.
    co_await when_all(nothing(20ms), nothing(40ms));
    std::cout << "all void tasks done\n";

    // 3. Vector of Task<T>: a vector of results in the original order.
    std::vector<Task<int>> many;
    for (int i = 1; i <= 4; ++i) many.push_back(number(i * 10, std::chrono::milliseconds(100 - i * 20)));
    std::vector<int> results = co_await when_all(std::move(many));
    std::cout << "vector:";
    for (int r : results) std::cout << " " << r;
    std::cout << "\n";

    // 4. Vector of Task<void>.
    std::vector<Task<void>> voids;
    voids.push_back(nothing(10ms));
    voids.push_back(nothing(20ms));
    co_await when_all(std::move(voids));

    // 5. Failures: every child is awaited first, then the first failure (in argument order) is rethrown.
    try {
        (void)co_await when_all(number(1, 100ms), failing());
    } catch (const std::runtime_error& ex) {
        std::cout << "when_all rethrew: " << ex.what() << "\n";
    }
}

int main() {
    app().wait();
    return 0;
}
