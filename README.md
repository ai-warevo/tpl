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
* **Combinators (`when_all` & `when_any`):** Supports both heterogeneous variadic tasks (`std::tuple` / `Task<void>` / winner index) and homogeneous `std::vector<Task<T>>`.
* **Cooperative Cancellation:** `CancellationTokenSource` & `CancellationToken` with `.cancel()`, `.cancel_after()`, and `OperationCanceledException`.

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

## 🚀 Quick Start

```cpp
#include <aw/tpl/tpl.hpp>
#include <iostream>

using namespace std::chrono_literals;
using namespace aw::tpl;
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

## ⚠️ C++20 Coroutine Lifetime Note

Per **C++ Core Guidelines (CP.51)**, avoid invoking capturing coroutine lambdas immediately (`[&]() -> Task<void> { co_await ...; }()`), as the temporary lambda closure is destroyed at the semicolon before the coroutine resumes. Instead:
* Pass state as explicit function/lambda parameters: `[](int& x) -> Task<void> { ... }(x)`, **or**
* Use `Task<>::run([&] { ... })`, which stores the callable safely inside the coroutine frame.

## 📄 License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for more information.
