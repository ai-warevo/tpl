# 34 — Add a way to schedule a handle on a specific pool

**Severity:** Low  
**Depends on:** 33 (`default-thread-count-atomic`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/thread_pool.hpp`
- `src/thread_pool.cpp`

## Exact Location
- `src/thread_pool.cpp` `detail::schedule_on_pool` (lines 7-10); header lines 33 and 79.

## Problem Description
1. `schedule_on_pool(h)` always chooses `current_pool` of the CALLING thread or the global pool. A coroutine resumed from a cancel callback (called on any thread) or a timer lands on the wrong pool; user-created pools lose affinity.

## Step-by-Step Instructions
1. Add an overload `schedule_on_pool(ThreadPool* pool, std::coroutine_handle<> h)` declared in the header (inside `detail`, a friend of `ThreadPool` like the existing one) that schedules on `pool` or, when `pool` is null, falls back to the existing behaviour. Implement it in the cpp and make the existing function call it.

## Unit Test & Acceptance Criteria
- No new test file in this task (the test is a separate task, `pool-affinity-api-test`). Acceptance: full suite passes unchanged.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
