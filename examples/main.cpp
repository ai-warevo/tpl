#include <aw/tpl/tpl.hpp>

#include <chrono>
#include <iostream>
#include <mutex>
#include <numeric>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using namespace aw::tpl;

std::mutex cout_mtx;

Task<int> simulate_io(int id, std::chrono::milliseconds delay, CancellationToken token = CancellationToken::none()) {
    co_await async_sleep(delay, token);
    co_return id * 10;
}

Task<std::string> fetch_user() {
    co_await async_sleep(200ms);
    co_return "ai-warevo";
}

Task<void> run_demo() {
    // 1. Heterogeneous when_all
    auto [val, user] = co_await when_all(
        simulate_io(1, 300ms),
        fetch_user()
    );
    std::cout << "when_all -> val: " << val << ", user: " << user << "\n";

    // 2. Heterogeneous when_any (returns index of winner)
    size_t winner = co_await when_any(
        simulate_io(10, 500ms),
        simulate_io(20, 150ms)
    );
    std::cout << "when_any winner index: " << winner << "\n";

    // 3. CancellationTokenSource with cancel_after
    CancellationTokenSource cts;
    cts.cancel_after(200ms);

    try {
        (void)co_await simulate_io(99, 2000ms, cts.token());
    } catch (const OperationCanceledException& ex) {
        std::cout << "Canceled as expected: " << ex.what() << "\n";
    }
}

int main() {
    ThreadPool::set_default_threads(4);
    run_demo().wait();
    return 0;
}
