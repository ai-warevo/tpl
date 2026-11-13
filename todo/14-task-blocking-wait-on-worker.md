# 14 — Blocking .result()/.wait() on a pool worker deadlocks (thread-pool starvation)

**Severity:** High  
**Depends on:** 08 (`task-empty-state-guard`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/task.hpp`
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/task.hpp` `Task::wait_until_ready` (lines 125-130), `wait` (85-88), `result` (96-99).

## Problem Description
1. A coroutine running on a pool worker that calls `inner.result()` blocks that worker on `done_sem.acquire()` (task_state.hpp line 19 semaphore). If the inner task needs a worker to run (or to resume after a timer) and all workers are blocked, nothing can make progress: deadlock. With `ThreadPool(1)` it is guaranteed: probe task never became ready.
2. Nothing in the API warns about it and the README advertises `.result()` freely.
3. `detail::current_pool` (thread_pool.hpp line 27) is non-null only on pool worker threads, so a blocking wait on a worker can be detected.

**Audit evidence:** Probe: `ThreadPool(1)` + outer coroutine calling `inner.result()` → outer never ready after 1 s.

## Step-by-Step Instructions
1. In `wait_until_ready`, after the null-state guard (task 08 (`task-empty-state-guard`)) and only when the task is NOT yet ready, check `::aw::tpl::detail::current_pool != nullptr` (use the fully qualified name; include `<aw/tpl/thread_pool.hpp>`). If so throw `std::logic_error` with message "Blocking wait on a thread-pool worker would deadlock; use co_await instead."
2. A ready task must still return immediately even on a worker (check readiness first).
3. Document the rule on `wait()` and `result()`.

## Unit Test & Acceptance Criteria
- In `tests/test_task.cpp` add TEST_CASE "Blocking wait on a pool worker throws instead of deadlocking" tagged `[task][deadlock]`: outer coroutine (running on the global pool) creates an inner `Task<int>` that sleeps 30 ms and returns 1, then calls `inner.result()` inside try/catch and returns 1 if `std::logic_error` was thrown else 0; REQUIRE the outer `.result()` from the main thread equals 1.
- Add TEST_CASE "result() on an already-ready task works on a worker": inner task `co_return 5` completed (poll `await_ready()` with a 2 s deadline) then outer calls `inner.result()` → REQUIRE 5.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
