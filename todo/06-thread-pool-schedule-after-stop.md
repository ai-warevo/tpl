# 06 — Scheduling after pool shutdown has begun strands the handle (no worker / timer left)

**Severity:** High  
**Depends on:** 05 (`thread-pool-flush-timers-on-shutdown`)

## Target Files (ONLY these may be edited)
- `src/thread_pool.cpp`
- `tests/test_thread_pool.cpp`

## Exact Location
- `src/thread_pool.cpp` `ThreadPool::schedule` (lines 48-54), `ThreadPool::schedule_after` (lines 56-63).

## Problem Description
1. During or after destruction a resumed coroutine can call `async_sleep` (lands in `schedule_after`, pushes into `timers_` that no thread reads any more) or a handle can be passed to `schedule` after every worker has exited.
2. Such handles are never run; the awaiting `Task` blocks forever. Example: task 05 (`thread-pool-flush-timers-on-shutdown`) flushes a timer; the resumed coroutine immediately awaits another sleep.

## Step-by-Step Instructions
1. In `schedule_after`: take `timer_mtx_`, and if `timer_thread_.get_stop_token().stop_requested()` is true, release the lock and call `schedule(h)` immediately (ignore the duration). Otherwise push the entry as today (the check and the push must be under the same lock so a handle can never be added after the final flush of task 05 (`thread-pool-flush-timers-on-shutdown`)).
2. In `schedule`: take `queue_mtx_`; if the worker threads have been asked to stop (test `workers_.front().get_stop_token().stop_requested()` when `workers_` is non-empty), release the lock and resume the handle inline on the calling thread (`h.resume()`) instead of enqueuing. Add a comment that this only happens during shutdown.
3. Make sure the inline resume happens AFTER the lock is released.

## Unit Test & Acceptance Criteria
- In `tests/test_thread_pool.cpp` add TEST_CASE "Sleep started during shutdown completes immediately" tagged `[thread_pool][dtor]`: single-worker pool; start a `Task<int>` that sleeps 5000 ms, then sleeps 5000 ms again, then returns 9; destroy the pool; REQUIRE `result() == 9` (must finish quickly; use ctest timeout 30).
- Rerun the 300-iteration stress test from task 04 (`thread-pool-dtor-join-order`): still passes.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
