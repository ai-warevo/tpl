# 36 — Cancelled sleep should resume on the pool the coroutine was running on

**Severity:** Low  
**Depends on:** 34 (`pool-affinity-api`), 24 (`cancel-sleep-unregister-on-resume`)

## Target Files (ONLY these may be edited)
- `src/cancellation.cpp`
- `tests/test_cancellation.cpp`

## Exact Location
- `src/cancellation.cpp` `await_suspend` lambda `try_resume` (lines 102-106).

## Problem Description
1. `try_resume` calls `schedule_on_pool(h)` from whichever thread fires it: the cancelling thread (maybe not a pool thread → resumes on the global pool) or the timer. A coroutine started on a custom pool migrates to the global pool after a cancelled sleep.

## Step-by-Step Instructions
1. In `await_suspend` capture `detail::current_pool` into a local at the top and use the new `schedule_on_pool(pool, h)` overload inside `try_resume`.

## Unit Test & Acceptance Criteria
- In `tests/test_cancellation.cpp` add TEST_CASE "Cancelled sleep resumes on the original pool" tagged `[cancellation][affinity]`: local single-worker pool; start the coroutine under that pool; cancel from the main thread; the coroutine records `detail::current_pool` after the catch; REQUIRE it equals the local pool.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
