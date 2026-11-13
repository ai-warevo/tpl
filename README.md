# ⚡ aw::tpl

[![C++20](https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white)](https://cppreference.com)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
<br />
[![CI](https://github.com/ai-warevo/tpl/actions/workflows/ci.yml/badge.svg)](https://github.com/ai-warevo/tpl/actions/workflows/ci.yml)
<br />
[![Tests](https://github.com/ai-warevo/tpl/actions/workflows/test.yml/badge.svg)](https://github.com/ai-warevo/tpl/actions/workflows/test.yml)
[![Coverage](https://raw.githubusercontent.com/ai-warevo/tpl/refs/heads/badges/coverage.svg)](https://ai-warevo.github.io/tpl)

> **C# Task Parallel Library (TPL) ergonomics brought to modern C++20 coroutines.**  
> *Part of the **[ai-warevo](https://github.com/ai-warevo)** ecosystem — evoked into existence via prompt engineering.* 🪄

`aw::tpl` is a lightweight, zero-dependency C++20 coroutine library providing eager `Task<T>`, a background `ThreadPool` with non-blocking async timers, `when_all`, `when_any`, `Task<>::run`, and cooperative `CancellationToken` support.

## ✨ Features

* **Eager Coroutines (`Task<T>` / `Task<void>`):** Tasks start executing immediately on the thread pool upon invocation — just like `System.Threading.Tasks.Task` in C#.
* **Thread Pool Offloading (`Task<>::run`):** Safely execute synchronous or capturing lambdas on worker threads (`Task<>::run([&] { ... })`, equivalent to `Task.Run`).
* **Clean Synchronization:** `.result()` for `Task<T>` and `.wait()` for `Task<void>` enforced at compile time via C++20 `requires`.
* **Non-Blocking Timers (`async_sleep`):** Dedicated timer thread with a min-heap priority queue — 10,000 concurrent delays never block worker threads.
* **Combinators (`when_all` & `when_any`):** Supports both heterogeneous variadic tasks and homogeneous `std::vector<Task<T>>`. `when_any` returns `Task<Task<T>>` exactly like .NET's `Task.WhenAny`.
* **Cooperative Cancellation & Timeouts:** `CancellationTokenSource` & `CancellationToken` with `.cancel()`, `.cancel_after()`, `CancellationTokenSource(timeout)`, `OperationCanceledException` and `TimeoutException`.

## 📦 Integration via CMake `FetchContent`

```cmake
include(FetchContent)
FetchContent_Declare(
    aw_tpl
    GIT_REPOSITORY https://github.com/ai-warevo/tpl.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(aw_tpl)

target_link_libraries(your_target PRIVATE aw::tpl)
```

## 🗂️ Project Layout (mirrors .NET)

Namespaces map 1:1 to directories (`include/aw/tpl/...` and `src/...`), with roughly one public entity per header:

| C++ namespace / directory | .NET namespace | Contents |
| --- | --- | --- |
| `aw::tpl` — `aw/tpl/` | `System` | `OperationCanceledException`, `TimeoutException` |
| `aw::tpl::threading` — `aw/tpl/threading/` | `System.Threading` | `ThreadPool`, `CancellationToken`, `CancellationTokenSource` |
| `aw::tpl::threading::tasks` — `aw/tpl/threading/tasks/` | `System.Threading.Tasks` | `Task<T>`, `Task<void>`, `TaskAlreadyMovedException`, `when_all`, `when_any`, `async_sleep` (`Task.Delay`) |
| `*::detail` — `*/detail/` | *(internal)* | task state, promises, awaiters, scheduling helpers, cancellation state — not part of the public API |

Include `<aw/tpl/tpl.hpp>` for everything, or individual headers such as `<aw/tpl/threading/tasks/task.hpp>`.
Non-template code lives in `src/`, mirroring the header tree.

## 🚀 Quick Start

```cpp
#include <aw/tpl/tpl.hpp>
#include <iostream>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading;
using namespace aw::tpl::threading::tasks;

Task<int> compute_async(int x, CancellationToken token) {
    co_await async_sleep(300ms, token);
    co_return x * 2;
}

Task<void> app() {
    CancellationTokenSource cts;
    cts.cancel_after(1000ms);

    // 1. Run async coroutines concurrently via when_all
    auto [a, b] = co_await when_all(
        compute_async(10, cts.token()),
        compute_async(21, cts.token())
    );

    // 2. Offload a capturing lambda safely via Task<>::run
    int bonus = co_await Task<>::run([&]() {
        return a + b;
    });

    std::cout << "Result: " << bonus << "\n"; // 62
}

int main() {
    // by default it's equal to the number of hardware threads
    // static inline size_t default_thread_count_ = std::thread::hardware_concurrency();
    ThreadPool::set_default_threads(4);
    app().wait();
    return 0;
}
```

## 🔀 `when_all` / `when_any`

```cpp
// when_all: results in argument order (Task<void> children become std::monostate).
auto [n, text] = co_await when_all(compute(1), fetch_text());
std::vector<int> all = co_await when_all(std::move(vector_of_tasks));
```

`when_any` mirrors .NET's `Task.WhenAny`: it returns `Task<Task<T>>`. The first `co_await` yields the
**winning task**, the second one unwraps its value (or rethrows its exception):

```cpp
// C#:  Task<string> completed = await Task.WhenAny(task1, task2); cts.Cancel(); string r = await completed;
CancellationTokenSource cts;
Task<std::string> task1 = fetch_from_server_a(cts.token());
Task<std::string> task2 = fetch_from_server_b(cts.token());

Task<std::string> completed = co_await when_any(task1, task2);   // wait for the first one to finish
cts.cancel();                                                      // cancel the remaining task
std::string result = co_await completed;                           // value, or throws if the winner failed

if (completed == task1) { /* identity check, like reference equality in C# */ }
```

| Call | Returns | .NET equivalent |
| --- | --- | --- |
| `when_any(Task<T>, Task<T>, ...)` (same `T`) | `Task<Task<T>>` | `Task.WhenAny<T>(...)` |
| `when_any(std::vector<Task<T>>)` | `Task<Task<T>>` | `Task.WhenAny<T>(IEnumerable<Task<T>>)` |
| `when_any(Task<A>, Task<B>, ...)` (mixed) | `Task<Task<void>>` | `Task.WhenAny(params Task[])` → `Task<Task>` |

* A failed/canceled task also counts as completed: the outer task succeeds and the failure surfaces on the second `co_await`.
* `Task<T>` converts implicitly to `Task<void>` (the non-generic `Task` of .NET) sharing the same state, and `operator==` compares task identity across types, so for mixed types you can write `if (winner == slow_int_task)` and then `co_await slow_int_task` to read the typed value.
* `when_any` never consumes the winner's result and does not cancel the losers; use a shared `CancellationTokenSource` as shown above.

## ⏱️ Timeouts

```cpp
// C#:  using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(3));
//      try { var data = await LongRunningOperationAsync(cts.Token); }
//      catch (OperationCanceledException) { throw new TimeoutException("The operation timed out."); }
Task<std::string> fetch_with_timeout() {
    CancellationTokenSource cts(3s);                 // cancels itself after 3 seconds
    try {
        co_return co_await long_running_operation_async(cts.token());
    } catch (const OperationCanceledException&) {
        throw TimeoutException("The operation timed out.");
    }
}
```

`CancellationTokenSource(std::chrono::milliseconds)` is the same as `cancel_after(delay)` right after construction.

## 📚 Examples

Every part of the public API is shown in [`examples/`](examples):

| Example | Covers |
| --- | --- |
| [`01_tasks_basics.cpp`](examples/01_tasks_basics.cpp) | `Task<T>`/`Task<void>`, `co_await`, `wait()`/`result()`, exceptions, move-only results |
| [`02_task_run.cpp`](examples/02_task_run.cpp) | `Task<>::run` with lambdas, functors and move-only callables |
| [`03_when_all.cpp`](examples/03_when_all.cpp) | variadic and vector `when_all`, `Task<void>` children, failure handling |
| [`04_when_any.cpp`](examples/04_when_any.cpp) | `Task<Task<T>>` winners, `when_any` + cancel the rest, mixed types, failed winners, empty input |
| [`05_cancellation.cpp`](examples/05_cancellation.cpp) | `CancellationTokenSource`, tokens, callbacks, `cancel_after`, `async_sleep` |
| [`06_thread_pool.cpp`](examples/06_thread_pool.cpp) | `ThreadPool::global`, `set_default_threads`, private pools, blocking guard |
| [`07_timeout.cpp`](examples/07_timeout.cpp) | `CancellationTokenSource(timeout)`, `OperationCanceledException` → `TimeoutException` |

## ⚠️ C++20 Coroutine Lifetime Note

Per **C++ Core Guidelines (CP.51)**, avoid invoking capturing coroutine lambdas immediately (`[&]() -> Task<void> { co_await ...; }()`), as the temporary lambda closure is destroyed at the semicolon before the coroutine resumes. Instead:
* Pass state as explicit function/lambda parameters: `[](int& x) -> Task<void> { ... }(x)`, **or**
* Use `Task<>::run([&] { ... })`, which stores the callable safely inside the coroutine frame.

## 🧵 Threading & Lifetime Contract

* **Never block on a pool worker.** `.result()` / `.wait()` on an unfinished task from a pool worker thread throws `std::logic_error` (it would starve or deadlock the pool). Use `co_await` inside coroutines; block only from non-pool threads such as `main`.
* **Pool shutdown is orderly.** Destroying a `ThreadPool` resumes pending timers immediately (sleeps end early), drains queued work, then joins all threads. Work scheduled after shutdown began runs inline on the scheduling thread.
* **Cancellation callbacks** run synchronously on the thread that calls `cancel()` (or on the timer thread for `cancel_after`). All callbacks run even if one throws; the first exception is rethrown by `cancel()`. `register_callback` returns an id usable with `unregister_callback`.
* **Several awaiters per task are allowed.** A move-only result (e.g. `Task<std::unique_ptr<T>>`) can be taken once; a second `result()` / `co_await` throws `TaskAlreadyMovedException`. `when_any` never consumes the result of its winner.
* **`when_all` waits for every child** and then rethrows the first failure (in argument/index order), so no child keeps running unobserved.
* **`when_any` does not cancel the losers**; they keep running to completion.
* **`Task` is `[[nodiscard]]`**: dropping a task silently loses its exception.
* **Build options:** `AW_TPL_SANITIZE=address|thread`, `AW_TPL_WARNINGS_AS_ERRORS`, `AW_TPL_ENABLE_COVERAGE`, `AW_TPL_INSTALL`.

## 📄 License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for more information.
