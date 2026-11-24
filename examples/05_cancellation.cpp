// Cooperative cancellation: CancellationTokenSource, tokens, callbacks, cancel_after, async_sleep.
#include <aw/tpl/tpl.hpp>

#include <atomic>
#include <format>
#include <memory>
#include <print>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading;
using namespace aw::tpl::threading::tasks;

Task<int> long_operation(CancellationToken token) {
    co_await async_sleep(5000ms, token);
    co_return 1;
}

Task<int> polling_loop(CancellationToken token) {
    int iterations = 0;
    while (true) {
        token.throw_if_cancellation_requested();
        co_await async_sleep(10ms);
        ++iterations;
        if (iterations == 1000) co_return iterations;
    }
}

// Helper: normalises bool to lowercase "true"/"false".
inline std::string bool_str(bool v) { return std::format("{}", v ? "true" : "false"); }

int main() {
    // 1. Cancel from the outside.
    {
        CancellationTokenSource cts;
        auto op = long_operation(cts.token());
        cts.cancel();
        try {
            (void)op.result();
        } catch (const OperationCanceledException& ex) {
            std::print("1. canceled: {}\n", ex.what());
        }
    }

    // 2. Cancel automatically after a timeout.
    {
        CancellationTokenSource cts;
        cts.cancel_after(100ms);
        try {
            (void)long_operation(cts.token()).result();
        } catch (const OperationCanceledException&) {
            std::print("2. canceled by cancel_after(100ms)\n");
        }
    }

    // 3. Manual polling with throw_if_cancellation_requested / is_cancellation_requested.
    {
        CancellationTokenSource cts;
        cts.cancel_after(50ms);
        try {
            (void)polling_loop(cts.token()).result();
        } catch (const OperationCanceledException&) {
            std::print("3. polling loop stopped, requested = {}\n", bool_str(cts.token().is_cancellation_requested()));
        }
    }

    // 4. Callbacks: run on cancel(); unregister_callback removes one; late registrations run immediately.
    {
        CancellationTokenSource cts;
        CancellationToken token = cts.token();
        std::atomic<int> calls{0};

        auto id1 = token.register_callback([&] { ++calls; });
        auto id2 = token.register_callback([&] { calls += 100; });
        std::print("4. registered callbacks: {}\n", token.registered_callback_count());
        (void)token.unregister_callback(id2);

        cts.cancel();
        cts.cancel();
        (void)id1;
        token.register_callback([&] { calls += 10; });
        std::print("   callback total = {} (1 + 10)\n", calls.load());
    }

    // 5. A token that can never be canceled.
    {
        CancellationToken none = CancellationToken::none();
        std::print("5. none() canceled? {}\n", bool_str(none.is_cancellation_requested()));
        auto sleeper = [](CancellationToken t) -> Task<int> {
            co_await async_sleep(20ms, t);
            co_return 7;
        };
        std::print("   sleep with none(): {}\n", sleeper(none).result());
    }
    return 0;
}
