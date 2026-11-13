// when_any: race tasks and take the first one that finishes.
// Like C# Task.WhenAny it returns Task<Task<T>>: the first co_await gives you the *winning task*,
// the second co_await unwraps its value (or rethrows its exception).
#include <aw/tpl/tpl.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading;
using namespace aw::tpl::threading::tasks;

Task<std::string> fetch_from_server(std::string name, std::chrono::milliseconds latency, CancellationToken token) {
    co_await async_sleep(latency, token); // throws OperationCanceledException when canceled
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

        // Wait for the first task to finish.
        Task<std::string> completed_task = co_await when_any(task1, task2);

        // Cancel the remaining task to save resources.
        cts.cancel();

        // Unwrap the result (or throw if the winning task failed).
        std::string result = co_await completed_task;
        std::cout << "1. " << result << (completed_task == task2 ? " (task2 won)" : " (task1 won)") << "\n";

        try {
            (void)co_await task1; // the loser was canceled
        } catch (const OperationCanceledException&) {
            std::cout << "   task1 was canceled\n";
        }
    }

    // 2. Vector overload: same shape, the winner is a Task<T> from the vector.
    {
        std::vector<Task<int>> racers;
        racers.push_back(number(100, 300ms));
        racers.push_back(number(200, 40ms));
        racers.push_back(number(300, 150ms));
        Task<int> first = co_await when_any(racers);
        std::cout << "2. winner is racers[1]? " << std::boolalpha << (first == racers[1])
                  << ", value: " << co_await first << "\n";
    }

    // 3. Mixed result types give Task<Task<void>> (C# Task.WhenAny(params Task[]) -> Task<Task>).
    //    Compare the winner with your typed handles, then await the typed handle to get the value.
    {
        Task<int> slow = number(1, 300ms);
        Task<void> quick = nothing(30ms);
        Task<std::string> text = fetch_from_server("C", 200ms, CancellationToken::none());

        Task<void> winner = co_await when_any(slow, quick, text);
        std::cout << "3. winner is `quick`? " << (winner == quick) << "\n";
        co_await winner;               // observe completion / rethrow failure
        co_await when_all(slow, text); // the losers keep running; wait for them if you care
    }

    // 4. A failed task also counts as completed; the failure appears on the second await.
    {
        std::vector<Task<int>> with_failure;
        with_failure.push_back(number(1, 200ms));
        with_failure.push_back(failing(10ms));
        Task<int> completed = co_await when_any(with_failure); // does not throw
        try {
            (void)co_await completed; // throws here
        } catch (const std::runtime_error& ex) {
            std::cout << "4. winner failed with: " << ex.what() << "\n";
        }
        co_await nothing(200ms); // let the slow loser finish before leaving the scope
    }

    // 5. An empty vector is an error.
    try {
        (void)co_await when_any(std::vector<Task<int>>{});
    } catch (const std::invalid_argument& ex) {
        std::cout << "5. empty vector: " << ex.what() << "\n";
    }
}

int main() {
    app().wait();
    return 0;
}
