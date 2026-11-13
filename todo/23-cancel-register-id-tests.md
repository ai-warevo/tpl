# 23 — Tests for registration ids / unregister

**Severity:** Medium  
**Depends on:** 22 (`cancel-register-id-api`)

## Target Files (ONLY these may be edited)
- `tests/test_cancellation.cpp`

## Exact Location
- `tests/test_cancellation.cpp` (append at end).

## Problem Description
1. The new API from `cancel-register-id-api` has no tests.

## Step-by-Step Instructions
1. Append TEST_CASEs (tag `[cancellation][register]`): (1) register returns non-zero ids that differ; `registered_callback_count()` is 2; (2) `unregister_callback(id)` returns true once and false the second time, count drops; (3) an unregistered callback does NOT run on `cancel()` while the other does; (4) registering after cancel runs immediately and returns 0; (5) `CancellationToken::none().register_callback(...)` returns 0 and count is 0; (6) unregistering an id from inside another callback during `cancel()` does not deadlock (the state lock is not held while callbacks run).

## Unit Test & Acceptance Criteria
- All six cases pass; coverage of the new functions 100%.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
