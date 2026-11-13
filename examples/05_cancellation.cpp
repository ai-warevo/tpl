// Cooperative cancellation: CancellationTokenSource, tokens, callbacks, cancel_after, async_sleep.
#include <aw/tpl/tpl.hpp>

#include <atomic>
#include <iostream>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading::tasks;

Task<int> long_operation(CancellationToken token) {
    co_await async_sleep(5000ms, token);   // throws OperationCanceledException when canceled
    co_return 1;
}

Task<int> polling_loop(CancellationToken token) {
    int iterations = 0;
    while (true) {
        token.throw_if_cancellation_requested();   // manual cooperative check
        co_await async_sleep(10ms);
        ++iterations;
        if (iterations == 1000) co_return iterations;
    }
}

int main() {
    // 1. Cancel from the outside.
    {
        CancellationTokenSource cts;
        auto op = long_operation(cts.token());
        cts.cancel();
        try {
            (void)op.result();
        } catch (const OperationCanceledException& ex) {
            std::cout << "1. canceled: " << ex.what() << "\n";
        }
    }

    // 2. Cancel automatically after a timeout.
    {
        CancellationTokenSource cts;
        cts.cancel_after(100ms);
        try {
            (void)long_operation(cts.token()).result();
        } catch (const OperationCanceledException&) {
            std::cout << "2. canceled by cancel_after(100ms)\n";
        }
    }

    // 3. Manual polling with throw_if_cancellation_requested / is_cancellation_requested.
    {
        CancellationTokenSource cts;
        cts.cancel_after(50ms);
        try {
            (void)polling_loop(cts.token()).result();
        } catch (const OperationCanceledException&) {
            std::cout << "3. polling loop stopped, requested = " << std::boolalpha
                      << cts.token().is_cancellation_requested() << "\n";
        }
    }

    // 4. Callbacks: run on cancel(); unregister_callback removes one; late registrations run immediately.
    {
        CancellationTokenSource cts;
        CancellationToken token = cts.token();
        std::atomic<int> calls{0};

        auto id1 = token.register_callback([&] { ++calls; });
        auto id2 = token.register_callback([&] { calls += 100; });
        std::cout << "4. registered callbacks: " << token.registered_callback_count() << "\n";
        (void)token.unregister_callback(id2);

        cts.cancel();
        cts.cancel();   // idempotent
        (void)id1;
        token.register_callback([&] { calls += 10; });   // already canceled: runs right away
        std::cout << "   callback total = " << calls.load() << " (1 + 10)\n";
    }

    // 5. A token that can never be canceled.
    {
        CancellationToken none = CancellationToken::none();
        std::cout << "5. none() canceled? " << std::boolalpha << none.is_cancellation_requested() << "\n";
        std::cout << "   sleep with none(): ";
        auto sleeper = [](CancellationToken t) -> Task<int> { co_await async_sleep(20ms, t); co_return 7; };
        std::cout << sleeper(none).result() << "\n";
    }
    return 0;
}
