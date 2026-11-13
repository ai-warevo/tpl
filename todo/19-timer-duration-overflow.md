# 19 — async_sleep with huge duration overflows steady_clock arithmetic (UB, fires immediately)

**Severity:** Medium  
**Depends on:** 05 (`thread-pool-flush-timers-on-shutdown`)

## Target Files (ONLY these may be edited)
- `src/thread_pool.cpp`
- `tests/test_thread_pool.cpp`

## Exact Location
- `src/thread_pool.cpp` `ThreadPool::schedule_after` (lines 56-63), line 57 `std::chrono::steady_clock::now() + duration`.

## Problem Description
1. `std::chrono::milliseconds::max()` is converted to nanoseconds when added to `time_point` → signed overflow (UB). UBSan probe: `signed integer overflow: 9223372036854775807 * 1000000`. The wrapped wake time is in the past, so the coroutine resumes immediately instead of sleeping ~forever.
2. Negative durations are handled by `await_ready` but `schedule_after` can also be reached directly.

**Audit evidence:** UBSan probe output: `chrono.h:227: signed integer overflow`; task reported ready after 300 ms.

## Step-by-Step Instructions
1. In `schedule_after` clamp `duration` before the addition: values `<= 0` become zero; values above a safe maximum (use 100 years expressed in hours, converted with `duration_cast`) become that maximum. Perform the comparison on `std::chrono::milliseconds` BEFORE converting to a finer unit.
2. Document the clamp in a comment.

## Unit Test & Acceptance Criteria
- In `tests/test_thread_pool.cpp` add TEST_CASE "async_sleep with maximum duration does not fire immediately" tagged `[thread_pool][sleep][overflow]`: start a `Task<void>` that awaits `async_sleep(std::chrono::milliseconds::max())`; wait 100 ms; REQUIRE the task is NOT ready (`await_ready()` false). Leave it pending (it is flushed at pool shutdown; requires the shutdown tasks).
- Run under UBSan: no overflow report.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
