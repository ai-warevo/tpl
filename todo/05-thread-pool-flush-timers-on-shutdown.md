# 05 — Shutdown must not drop pending timers or queued work (suspended coroutines hang forever)

**Severity:** High  
**Depends on:** 04 (`thread-pool-dtor-join-order`)

## Target Files (ONLY these may be edited)
- `src/thread_pool.cpp`
- `tests/test_thread_pool.cpp`

## Exact Location
- `src/thread_pool.cpp` `ThreadPool::timer_loop` (lines 86-108: `if (st.stop_requested()) return;` line 99, timers left in `timers_`), `ThreadPool::worker_loop` (lines 65-84: loop condition line 71, early `return` line 77), destructor (lines 36-42).

## Problem Description
1. When the destructor stops the timer thread, every entry still in `timers_` is discarded. The coroutine handle inside is never resumed and never destroyed: the coroutine frame leaks and any `Task` waiting on it (`wait()`/`result()`) blocks forever.
2. `worker_loop` tests `st.stop_requested()` in its `while` condition (line 71) before taking work, so a worker can exit while `tasks_` still contains handles. Those coroutines are also never resumed.
3. Probe (outside repo): a coroutine sleeping 200 ms on a pool that was destroyed after creation stayed not-ready forever.

## Step-by-Step Instructions
1. After the timer thread has been joined (this order is established by task 04 (`thread-pool-dtor-join-order`)), move every remaining entry of `timers_` into the work queue `tasks_` under `queue_mtx_` (no waiting for its deadline; drain with a loop popping the top). Do this in the destructor, before any worker is asked to stop.
2. In `worker_loop` change the loop so that a worker keeps taking work while `tasks_` is non-empty even if stop was requested, and only returns when stop is requested AND `tasks_` is empty. (The wait predicate on line 75 already allows this; fix the outer `while` condition on line 71 and the early return on line 77 so they cannot leave work behind.)
3. Do not resume pending timers inline in the destructor; always go through `tasks_` so the normal worker path runs them.
4. Add a comment stating the shutdown contract: pending sleeps wake immediately at pool destruction; coroutines resumed during destruction run to their next suspension point.

## Unit Test & Acceptance Criteria
- In `tests/test_thread_pool.cpp` add TEST_CASE "Pool destruction resumes coroutines suspended on timers" tagged `[thread_pool][dtor][flush]`: create `ThreadPool pool(1)` in an inner scope and, via `detail::current_pool = &pool` (restore to nullptr immediately), start a `Task<int>` that does `co_await async_sleep(5000ms)` and returns 5. Move the Task out of the scope (declare `Task<int>` before the scope). Sleep-free: leave the scope, then REQUIRE `task.result() == 5` finishes within the test (use `ctest --timeout 30`).
- Add TEST_CASE "Pool destruction drains queued work" tagged `[thread_pool][dtor][flush]`: single worker pool; start 100 `Task<void>` that each increment a shared `std::atomic<int>` (passed by reference); destroy the pool; REQUIRE the counter equals 100.
- Note: the coroutine started right at pool destruction may call `async_sleep` after the flush; task 06 (`thread-pool-schedule-after-stop`) makes that case safe.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
