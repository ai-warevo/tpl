# 04 — ThreadPool destructor: explicit stop under lock and explicit joins (members destroyed while threads still run)

**Severity:** Critical  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `src/thread_pool.cpp`
- `tests/test_thread_pool.cpp`

## Exact Location
- `src/thread_pool.cpp` `ThreadPool::~ThreadPool` (lines 36-42).
- `include/aw/tpl/thread_pool.hpp` member declaration order (lines 123-131): `workers_`, `tasks_`, `queue_mtx_`, `queue_cv_`, `timer_thread_`, `timers_`, `timer_mtx_`, `timer_cv_`.

## Problem Description
1. The destructor only calls `request_stop()` and `notify_all()` and then returns. The `std::jthread` members join in their own destructors, which run in reverse declaration order.
2. `timer_thread_` is declared BEFORE `timers_`, `timer_mtx_` and `timer_cv_`, so those three are destroyed first while the timer thread may still be inside `timer_loop` using them. Likewise `workers_` is declared before `queue_mtx_` / `queue_cv_`, so the mutex and condition variable die while workers may still wait on them. Destroying a mutex/condvar that is in use is undefined behaviour.
3. Lost wake-up: `request_stop()` and `notify_all()` happen without holding `queue_mtx_` / `timer_mtx_`. A worker can evaluate its wait predicate (stop not requested, queue empty), then the destructor sets stop and notifies before the worker blocks; the worker sleeps forever and the join hangs.
4. The global pool (`ThreadPool::global`, lines 20-23) takes this path at process exit.

**Audit evidence:** Static analysis of declaration order vs. destructor body; no existing test destroys a pool while a worker is blocked on the condition variable between predicate check and wait.

## Step-by-Step Instructions
1. Rewrite the destructor body so that it does NOT rely on member destruction order. Step A: lock `timer_mtx_`, call `timer_thread_.request_stop()`, unlock, then `timer_cv_.notify_all()`; then call `timer_thread_.join()` if `joinable()`. Step B: lock `queue_mtx_`, call `request_stop()` on every worker, unlock, then `queue_cv_.notify_all()`; then `join()` every joinable worker.
2. Do the timer thread first, completely, before touching the workers (task 05 (`thread-pool-flush-timers-on-shutdown`) relies on this order to flush pending timers into the work queue).
3. Do not change the header in this task; the order problem is made harmless because all threads are already joined when the member destructors run.
4. Do not edit the header's doc comments in this task (the header is not in the Target Files); the class documentation is updated by task 18 (`thread-pool-ctor-exception-safety`).

## Unit Test & Acceptance Criteria
- In `tests/test_thread_pool.cpp` add TEST_CASE "ThreadPool can be created and destroyed repeatedly with pending work" tagged `[thread_pool][dtor][stress]`: loop 300 iterations; in each iteration construct `ThreadPool pool(2)`, then enqueue work through the pool by setting `detail::current_pool = &pool` (restore it to nullptr right after, ideally via a small RAII helper local to the test) and starting one `Task<void>` that sleeps 10 s and one that sleeps 1 ms, then let the pool go out of scope. REQUIRE nothing; the pass condition is that the loop finishes.
- Run the whole test binary under ThreadSanitizer when available (`-fsanitize=thread`) and under ASan; there must be no report. Run ctest with `--timeout 60` to detect hangs.
- All existing `[thread_pool][dtor]` tests still pass.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
