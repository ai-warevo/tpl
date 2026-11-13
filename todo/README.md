The audit is finished. There's a `todo/` directory with `00-overview.md` and 46 task files, `01-…` to `46-…`. I didn't modify any existing source, header, test or CMake file. `git status` shows only the new `todo/` directory.

Every task edits at most two files, normally one source file plus its test file. The tasks contain no code snippets. I wrote them as prose steps and Catch2 test descriptions.

**Severity counts**
- **Critical (4):**
  - The `TaskState` continuation slot holds only one waiter, so a second awaiter on the same task is never resumed.
  - `complete()` runs inside `return_value`, so a waiter wakes before the coroutine's locals are destroyed, and deep await chains can overflow the stack.
  - `~ThreadPool` destroys its mutexes and condition variables before the timer and worker threads are joined, and it can lose a wake-up.
- **High (13):**
  - Pending timers and queued work are dropped at pool shutdown.
  - Work scheduled after shutdown starts is never run.
  - The `when_any` winner index is written after `completed` is published.
  - A default-constructed Task dereferences null.
  - `Task<T>` needs a default-constructible `T`.
  - `co_return {…}` doesn't compile.
  - `when_all` over move-only results doesn't compile.
  - A second `result()` on a move-only task silently returns null.
  - Calling `.result()` from a pool worker deadlocks.
  - The cancellable sleep awaiter reads its own members after the coroutine may have resumed.
  - Both `when_all` overloads rethrow while sibling tasks are still running.
- **Medium (15)** covers:
  - Thread-pool constructor exception safety.
  - Timer overflow for huge sleep durations.
  - A throwing cancellation callback skipping the remaining callbacks.
  - A moved-from `CancellationTokenSource` dereferencing null.
  - Cancellation callbacks accumulating on long-lived tokens.
  - `noexcept` awaiters that can terminate.
  - Flaky or misleading tests.
  - A CMake sanitizer option and a CI sanitizer job.
- **Low (14)** covers:
  - Atomic default thread count.
  - Pool affinity.
  - `[[nodiscard]]` markers and missing includes.
  - Warning, feature-check and install options in CMake.
  - README contract notes.
  - Windows test discovery.

**What was verified**
- I reproduced these with throw-away programs in `/tmp` using GCC 14 with ASan and UBSan:
  - the multi-awaiter hang
  - early waiter release before locals are destroyed
  - null dereference on an empty Task
  - null dereference on a moved-from `CancellationTokenSource`
  - the nested-wait deadlock
  - pending timers lost at shutdown
  - the signed overflow with `milliseconds::max()`
  - a throwing cancel callback skipping later callbacks
  - the compile failures for non-default-constructible results, `co_return {…}` and `when_all` over move-only tasks
- A prototype of the Critical coroutine redesign fixed the multi-awaiter hang and the early release. The 400000-deep chain crashed at `-O2` before the change and passed after it. At `-O0` or under sanitizers I could only confirm depths up to 10000, so the deep-chain test in task 03 is restricted to optimised builds.
- The `when_any` race (task 07) comes from reading the code. ThreadSanitizer wouldn't start in this sandbox, so I never saw it fail. `todo/00-overview.md` says this.
- I only had GCC 14. Clang, MSVC and AppleClang issues, such as libc++ `jthread` support, are reasoned, not reproduced.

I also listed the findings I checked and dropped in `todo/00-overview.md`: the README quick-start compiles, and `InitialScheduleAwaiter` is safe. I deleted my scratch files in `/tmp`.

Start with [`todo/00-overview.md`](<todo/00-overview.md>). It lists every task in execution order, grouped by severity, with the files each one touches and its dependencies. Work in numeric order. For example, tasks 01 to 03 must land before the later `Task` changes.