# 37 — Edge cases of cancel_after not tested

**Severity:** Low  
**Depends on:** 21 (`cancel-source-moved-from`)

## Target Files (ONLY these may be edited)
- `tests/test_cancellation.cpp`

## Exact Location
- `src/cancellation.cpp` `CancellationTokenSource::cancel_after` (lines 75-94).

## Problem Description
1. Untested: zero/negative delay (cancels synchronously on the caller thread), second `cancel_after` shortening/lengthening, manual `cancel()` racing with the timer (timer's later `cancel()` must be a no-op), and destruction of the source before the timer fires (the timer holds a shared_ptr, so the token still becomes cancelled).

## Step-by-Step Instructions
1. Append TEST_CASEs tagged `[cancellation][timer]`: (1) `cancel_after(0ms)` and `cancel_after(-5ms)` → token cancelled before the call returns; (2) manual cancel at ~5 ms followed by waiting past a 20 ms timer: callback count stays 1; (3) source destroyed right after `cancel_after(20ms)`: the token (copied before) becomes cancelled within 2 s (poll); (4) two `cancel_after` calls (500 ms and 10 ms): cancelled within 2 s.

## Unit Test & Acceptance Criteria
- All pass, repeated 50 times with no failure.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
