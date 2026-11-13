# aw::tpl audit — master task index

Audit of `include/aw/tpl/`, `src/`, `tests/`, `CMakeLists.txt` (commit `742f831`). **No source file was modified by the audit.** Every task edits at most two files (a source/header file plus its test file, or a single file).

## How to execute

1. Work strictly in numeric order; each task lists its dependencies and only depends on lower numbers.
2. Edit ONLY the files in the task's *Target Files* section; if a step seems to require a third file, stop and report.
3. After each task: `make test`, then rebuild with ASan+UBSan and run again; commit before starting the next task.
4. Findings marked *probe* were reproduced with throw-away programs outside the repository (GCC 14, ASan/UBSan). ThreadSanitizer could not run in the audit sandbox, so data-race findings are based on code reading and are labelled as such.

## Index (recommended execution order)

### Critical

| # | Task | Files | Depends on |
|---|------|-------|-----------|
| 01 | [Support multiple awaiters on one Task (single continuation slot loses waiters)](01-task-state-multi-continuation.md) | `include/aw/tpl/threading/tasks/task_state.hpp`, `tests/test_task.cpp` | — |
| 02 | [Add TaskStateBase::finish() that completes the state and returns the continuation instead of resuming it](02-task-state-finish-api.md) | `include/aw/tpl/threading/tasks/task_state.hpp`, `tests/test_task.cpp` | 01 |
| 03 | [Complete the task from final_suspend (after locals are destroyed) using symmetric transfer](03-promise-final-awaiter.md) | `include/aw/tpl/threading/tasks/promise.hpp`, `tests/test_task.cpp` | 02 |
| 04 | [ThreadPool destructor: explicit stop under lock and explicit joins (members destroyed while threads still run)](04-thread-pool-dtor-join-order.md) | `src/thread_pool.cpp`, `tests/test_thread_pool.cpp` | — |

### High

| # | Task | Files | Depends on |
|---|------|-------|-----------|
| 05 | [Shutdown must not drop pending timers or queued work (suspended coroutines hang forever)](05-thread-pool-flush-timers-on-shutdown.md) | `src/thread_pool.cpp`, `tests/test_thread_pool.cpp` | 04 |
| 06 | [Scheduling after pool shutdown has begun strands the handle (no worker / timer left)](06-thread-pool-schedule-after-stop.md) | `src/thread_pool.cpp`, `tests/test_thread_pool.cpp` | 05 |
| 07 | [when_any: winner_index written after `completed` is published (data race / stale index)](07-when-any-winner-index-race.md) | `include/aw/tpl/when_any.hpp`, `tests/test_when_any.cpp` | — |
| 08 | [Default-constructed Task dereferences a null state (UB / crash)](08-task-empty-state-guard.md) | `include/aw/tpl/threading/tasks/task.hpp`, `tests/test_task.cpp` | — |
| 09 | [TaskState<T> requires default-constructible, assignable T (store the result in std::optional<T>)](09-task-state-optional-value.md) | `include/aw/tpl/threading/tasks/task_state.hpp`, `include/aw/tpl/threading/tasks/task.hpp` | 03 |
| 10 | [Promise::return_value: greedy forwarding template breaks `co_return {…}` and move-only/nondefault types](10-promise-return-by-value.md) | `include/aw/tpl/threading/tasks/promise.hpp`, `tests/test_task.cpp` | 09 |
| 11 | [await_resume copies vector<unique_ptr<…>> (is_copy_constructible_v lies) → when_all of move-only tasks does not compile](11-task-move-only-container-copy.md) | `include/aw/tpl/threading/tasks/task.hpp`, `tests/test_task.cpp` | 10 |
| 12 | [Add a one-shot `value_taken` guard to TaskState (state support for single-use move-only extraction)](12-task-value-taken-state.md) | `include/aw/tpl/threading/tasks/task_state.hpp`, `tests/test_task.cpp` | 09 |
| 13 | [Throw TaskAlreadyMovedException when a move-only result is extracted twice](13-task-value-taken-extract.md) | `include/aw/tpl/threading/tasks/task.hpp`, `tests/test_task.cpp` | 12, 11 |
| 14 | [Blocking .result()/.wait() on a pool worker deadlocks (thread-pool starvation)](14-task-blocking-wait-on-worker.md) | `include/aw/tpl/threading/tasks/task.hpp`, `tests/test_task.cpp` | 08 |
| 15 | [CancelableSleepAwaiter::await_suspend touches `this` after the coroutine may have resumed; awaiter cannot be reused](15-cancel-sleep-awaiter-lifetime.md) | `src/cancellation.cpp`, `tests/test_cancellation.cpp` | — |
| 16 | [when_all(vector): first failure is rethrown while sibling tasks are still running](16-when-all-vector-wait-siblings.md) | `include/aw/tpl/when_all.hpp`, `tests/test_when_all.cpp` | — |
| 17 | [when_all(variadic): argument evaluation order, early rethrow and lifetime of per-task helper tasks](17-when-all-variadic-wait-siblings.md) | `include/aw/tpl/when_all.hpp`, `tests/test_when_all.cpp` | 16 |

### Medium

| # | Task | Files | Depends on |
|---|------|-------|-----------|
| 18 | [ThreadPool constructor leaks running threads if a later thread fails to start; move thread members last](18-thread-pool-ctor-exception-safety.md) | `include/aw/tpl/thread_pool.hpp`, `src/thread_pool.cpp` | 06 |
| 19 | [async_sleep with huge duration overflows steady_clock arithmetic (UB, fires immediately)](19-timer-duration-overflow.md) | `src/thread_pool.cpp`, `tests/test_thread_pool.cpp` | 05 |
| 20 | [A throwing cancellation callback prevents the remaining callbacks from running (lost wake-ups)](20-cancel-callback-exception-isolation.md) | `src/cancellation.cpp`, `tests/test_cancellation.cpp` | — |
| 21 | [Moved-from CancellationTokenSource dereferences null (cancel/token/cancel_after)](21-cancel-source-moved-from.md) | `src/cancellation.cpp`, `tests/test_cancellation.cpp` | — |
| 22 | [Add registration ids and unregister_callback to CancellationToken (callbacks accumulate forever)](22-cancel-register-id-api.md) | `include/aw/tpl/cancellation.hpp`, `src/cancellation.cpp` | 20, 21, 15 |
| 23 | [Tests for registration ids / unregister](23-cancel-register-id-tests.md) | `tests/test_cancellation.cpp` | 22 |
| 24 | [async_sleep(token): remove the cancel callback when the timer wins](24-cancel-sleep-unregister-on-resume.md) | `src/cancellation.cpp`, `tests/test_cancellation.cpp` | 23 |
| 25 | [Replace fixed sleep_for waits in test_task.cpp and test_when_all.cpp (flaky on loaded CI)](25-test-fixed-sleeps-task-when.md) | `tests/test_task.cpp`, `tests/test_when_all.cpp` | — |
| 26 | [test_thread_pool.cpp destructor tests do not test what they claim; unsafe thread_local juggling; REQUIRE on worker threads](26-test-thread-pool-wrong-pool.md) | `tests/test_thread_pool.cpp` | — |
| 27 | [REQUIRE/assert macros executed inside coroutines on pool worker threads](27-test-when-all-require-on-workers.md) | `tests/test_when_all.cpp` | 25 |
| 28 | [Coverage blind spots for Task<>::run](28-test-task-run-gaps.md) | `tests/test_task.cpp` | 14 |
| 29 | [Task::await_suspend is `noexcept` but can throw (lock/allocation) → std::terminate](29-noexcept-awaiter-allocation.md) | `include/aw/tpl/threading/tasks/task.hpp`, `tests/test_task.cpp` | 01 |
| 30 | [WhenAnyAwaiter::await_suspend is `noexcept` but takes a mutex (can throw → std::terminate)](30-when-any-awaiter-noexcept.md) | `include/aw/tpl/when_any.hpp`, `tests/test_when_any.cpp` | — |
| 31 | [Add AW_TPL_SANITIZE option (address / thread) to CMake](31-cmake-sanitizer-option.md) | `CMakeLists.txt` | — |
| 32 | [CI: add sanitizer jobs](32-ci-sanitizer-job.md) | `.github/workflows/test.yml` | 31 |

### Low

| # | Task | Files | Depends on |
|---|------|-------|-----------|
| 33 | [default_thread_count_ is a plain static written and read from different threads](33-default-thread-count-atomic.md) | `include/aw/tpl/thread_pool.hpp`, `tests/test_thread_pool.cpp` | — |
| 34 | [Add a way to schedule a handle on a specific pool](34-pool-affinity-api.md) | `include/aw/tpl/thread_pool.hpp`, `src/thread_pool.cpp` | 33 |
| 35 | [Test schedule_on_pool(pool, handle) targets the requested pool](35-pool-affinity-api-test.md) | `tests/test_thread_pool.cpp` | 34 |
| 36 | [Cancelled sleep should resume on the pool the coroutine was running on](36-cancel-sleep-resume-on-origin-pool.md) | `src/cancellation.cpp`, `tests/test_cancellation.cpp` | 34, 24 |
| 37 | [Edge cases of cancel_after not tested](37-cancel-after-edge-tests.md) | `tests/test_cancellation.cpp` | 21 |
| 38 | [Silent `unhandled_exception() {}` in fire-and-forget bridge coroutines can hang waiters](38-bridge-unhandled-exception-policy.md) | `src/cancellation.cpp`, `include/aw/tpl/when_any.hpp` | 20 |
| 39 | [Missing includes / not self-contained headers](39-include-hygiene.md) | `include/aw/tpl/when_any.hpp`, `include/aw/tpl/thread_pool.hpp` | — |
| 40 | [Mark Task [[nodiscard]]](40-nodiscard-task.md) | `include/aw/tpl/threading/tasks/task.hpp`, `tests/test_task.cpp` | — |
| 41 | [Add [[nodiscard]] and noexcept where appropriate on cancellation/pool API](41-nodiscard-cancellation-pool-api.md) | `include/aw/tpl/cancellation.hpp`, `include/aw/tpl/thread_pool.hpp` | 24 |
| 42 | [CMake: opt-in strict warnings (AW_TPL_WARNINGS_AS_ERRORS)](42-cmake-warnings-option.md) | `CMakeLists.txt` | — |
| 43 | [CMake: fail early when the toolchain lacks jthread/stop_token/semaphore/coroutines](43-cmake-feature-check.md) | `CMakeLists.txt` | — |
| 44 | [CMake: install rules and package config run when used via FetchContent](44-cmake-install-top-level-only.md) | `CMakeLists.txt` | — |
| 45 | [Document the concurrency contract (blocking waits, shutdown, callbacks, single-use move-only)](45-api-docs-and-contract.md) | `README.md` | 13, 17, 05, 40 |
| 46 | [Windows registers the whole test binary as one ctest entry](46-ctest-windows-discovery.md) | `tests/CMakeLists.txt` | — |

## Findings reviewed and NOT turned into tasks

- README quick-start (`[&]` lambda capturing structured bindings) compiles on GCC 14; left as is.
- `InitialScheduleAwaiter::await_suspend` does not touch `this` after scheduling, so a worker destroying the frame concurrently is harmless.
- `Task::run` stores the callable by value in the coroutine frame (no dangling-lambda issue).
- `Task::wait_until_ready` acquire/release re-post pattern correctly supports several blocked waiters.
- Compiling the library and examples with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` on GCC 14 gives zero warnings.

## Known limits of this audit

- Clang, MSVC and AppleClang were not available; platform-specific findings (jthread support in libc++, `-fcoroutines` on old GCC) are reasoned, not reproduced.
- Deep-chain stack tests only guarantee symmetric transfer with optimisation and without sanitizers on GCC (verified in a prototype: -O2 400000-deep chain crashed before, passed after).
