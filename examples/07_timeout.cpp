// Timeouts: a CancellationTokenSource that cancels itself after a delay, converted to TimeoutException.
//
// C# equivalent:
//   using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(3));
//   try { string data = await LongRunningOperationAsync(cts.Token); }
//   catch (OperationCanceledException) { throw new TimeoutException("The operation timed out."); }
#include <aw/tpl/tpl.hpp>

#include <iostream>
#include <string>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading;
using namespace aw::tpl::threading::tasks;

Task<std::string> long_running_operation(std::chrono::milliseconds duration, CancellationToken token) {
    co_await async_sleep(duration, token);   // throws OperationCanceledException on timeout
    co_return "data";
}

// Runs the operation with a time limit; throws TimeoutException if the limit is exceeded.
Task<std::string> fetch_with_timeout(std::chrono::milliseconds work, std::chrono::milliseconds limit) {
    CancellationTokenSource cts(limit);   // like new CancellationTokenSource(TimeSpan)
    try {
        co_return co_await long_running_operation(work, cts.token());
    } catch (const OperationCanceledException&) {
        throw TimeoutException("The operation timed out.");
    }
}

Task<void> app() {
    // 1. Finishes in time.
    std::string data = co_await fetch_with_timeout(50ms, 1000ms);
    std::cout << "1. got: " << data << "\n";

    // 2. Exceeds the limit.
    try {
        (void)co_await fetch_with_timeout(5000ms, 100ms);
    } catch (const TimeoutException& ex) {
        std::cout << "2. timeout: " << ex.what() << "\n";
    }

    // 3. Timeout combined with manual cancellation: link both with one source.
    {
        CancellationTokenSource cts(2000ms);   // safety net
        auto op = long_running_operation(5000ms, cts.token());
        cts.cancel();                          // user pressed "cancel" earlier than the timeout
        try {
            (void)co_await op;
        } catch (const OperationCanceledException&) {
            std::cout << "3. canceled before the timeout\n";
        }
    }
}

int main() {
    app().wait();
    return 0;
}
