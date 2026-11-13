# ⚡ aw::tpl

[![CI](https://github.com/ai-warevo/tpl/actions/workflows/ci.yml/badge.svg)](https://github.com/ai-warevo/tpl/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white)](https://cppreference.com)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

> **C# Task Parallel Library (TPL) ergonomics brought to modern C++20 coroutines.**  
> *Part of the **[ai-warevo](https://github.com/ai-warevo)** ecosystem — evoked into existence via prompt engineering.* 🪄

`aw::tpl` is a lightweight, zero-dependency C++20 coroutine library providing eager `Task<T>`, a background `ThreadPool` with non-blocking async timers, `when_all`, `when_any`, and cooperative `CancellationToken` support.

## ✨ Features

* **Eager Coroutines (`Task<T>` / `Task<void>`):** Tasks start executing immediately on the thread pool upon invocation — just like `System.Threading.Tasks.Task` in C#.
* **Clean Synchronization:** `.result()` for `Task<T>` and `.wait()` for `Task<void>` enforced at compile time via C++20 `requires`.
* **Non-Blocking Timers (`async_sleep`):** Dedicated timer thread with a min-heap priority queue — 10,000 concurrent delays never block worker threads.
* **Combinators (`when_all` & `when_any`):** Supports both heterogeneous variadic tasks (`std::tuple` / winner index) and homogeneous `std::vector<Task<T>>`.
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

Task<int> compute_async(int x, CancellationToken token) {
    co_await async_sleep(300ms, token);
    co_return x * 2;
}

Task<void> app() {
    CancellationTokenSource cts;
    cts.cancel_after(1000ms);

    auto [a, b] = co_await when_all(
        compute_async(10, cts.token()),
        compute_async(21, cts.token())
    );

    std::cout << "Result: " << (a + b) << "\n"; // 62
}

int main() {
    ThreadPool::set_default_threads(4);
    app().wait();
    return 0;
}
```

## 📄 License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for more information.
