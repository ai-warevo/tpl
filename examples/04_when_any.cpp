// when_any: race tasks and take the first one that finishes.
// Like C# Task.WhenAny it returns Task<Task<T>>: the first co_await gives you the *winning task*,
// the second co_await unwraps its value (or rethrows its exception).
#include <aw/tpl/tpl.hpp>

#include <format>
#include <memory>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading;
using namespace aw::tpl::threading::tasks;

Task<std::string> fetch_from_server(std::string name, std::chrono::milliseconds latency, CancellationToken token) {
    co_await async_sleep(latency, token);
    co_return "response from " + name;
}

Task<int> number(int v, std::chrono::milliseconds delay) {
    co_await async_sleep(delay);
    co_return v;
}

Task<void> nothing(std::chrono::milliseconds delay) {
    co_await async_sleep(delay);
}

Task<int> failing(std::chrono::milliseconds delay) {
    co_await async_sleep(delay);
    throw std::runtime_error("failed first");
    co_return 0;
}

// Helper: normalises bool to lowercase "true"/"false".
inline std::string bool_str(bool v) { return std::format("{}", v ? "true" : "false"); }

Task<void> app() {
    // 1. The classic C# pattern:
    //      using var cts = new CancellationTokenSource();
    //      Task<string> task1 = FetchFromServerAAsync(cts.Token);
    //      Task<string> task2 = FetchFromServerBAsync(cts.Token);
    //      Task<string> completedTask = await Task.WhenAny(task1, task2);
    //      cts.Cancel();                       // cancel the remaining task to save resources
    //      string result = await completedTask;
    {
        CancellationTokenSource cts;

        Task<std::string> task1 = fetch_from_server("A", 800ms, cts.token());
        Task<std::string> task2 = fetch_from_server("B", 50ms, cts.token());

        Task<std::string> completed_task = co_await when_any(task1, task2);

        cts.cancel();

        std::string result = co_await completed_task;
        std::print("1. {}{} (task{} won)\n", result, bool_str(completed_task == task2), completed_task == task2 ? 2 : 1);

        try {
            (void)co_await task1;
        } catch (const OperationCanceledException&) {
            std::print("   task1 was canceled\n");
        }
    }

    // 2. Vector overload: same shape, the winner is a Task<T> from the vector.
    {
        std::vector<Task<int>> racers{
            number(100, 300ms),
            number(200, 40ms),
            number(300, 150ms),
        };
        Task<int> first = co_await when_any(racers);
        std::print("2. winner is racers[1]? {}, value: {}\n", bool_str(first == racers[1]), co_await first);
    }

    // 3. Mixed result types give Task<Task<void>> (C# Task.WhenAny(params Task[]) -> Task<Task>).
    //    Compare the winner with your typed handles, then await the typed handle to get the value.
    {
        Task<int> slow = number(1, 300ms);
        Task<void> quick = nothing(30ms);
        Task<std::string> text = fetch_from_server("C", 200ms, CancellationToken::none());

        Task<void> winner = co_await when_any(slow, quick, text);
        std::print("3. winner is `quick`? {}\n", bool_str(winner == quick));
        co_await winner;
        (void)co_await when_all(slow, text);
    }

    // 4. A failed task also counts as completed; the failure appears on the second await.
    {
        std::vector<Task<int>> with_failure{
            number(1, 200ms),
            failing(10ms),
        };
        Task<int> completed = co_await when_any(with_failure);
        try {
            (void)co_await completed;
        } catch (const std::runtime_error& ex) {
            std::print("4. winner failed with: {}\n", ex.what());
        }
        co_await nothing(200ms);
    }

    // 5. An empty vector is an error.
    try {
        (void)co_await when_any(std::vector<Task<int>>{});
    } catch (const std::invalid_argument& ex) {
        std::print("5. empty vector: {}\n", ex.what());
    }
}

int main() {
    app().wait();
    return 0;
}
