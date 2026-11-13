# 20 — A throwing cancellation callback prevents the remaining callbacks from running (lost wake-ups)

**Severity:** Medium  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `src/cancellation.cpp`
- `tests/test_cancellation.cpp`

## Exact Location
- `src/cancellation.cpp` `CancellationState::cancel` (lines 10-22), loop lines 19-21; `CancellationState::register_callback` immediate run lines 34-36.

## Problem Description
1. Callbacks are invoked in a plain loop (line 19). If one throws, the exception escapes `cancel()` and the callbacks after it never run. Sleeping coroutines registered later in the vector are never resumed → hang. Probe: second callback hits = 0.
2. On the `register_callback` already-cancelled path an exception propagates into the caller (in `await_suspend`).

**Audit evidence:** Probe output: `threw from cancel` / `second cb hits=0`.

## Step-by-Step Instructions
1. In `cancel()` run EVERY callback inside its own try/catch; remember the first `std::exception_ptr`; after the loop rethrow it (so `cancel()` still reports the error).
2. In `register_callback` immediate path keep propagating (no change) but document it.
3. Do not edit `include/aw/tpl/cancellation.hpp` in this task (it is not a Target File); the behaviour is documented later by task 45 (`api-docs-and-contract`).

## Unit Test & Acceptance Criteria
- In `tests/test_cancellation.cpp` add TEST_CASE "All cancellation callbacks run even if one throws" tagged `[cancellation][exceptions]`: register callback A that throws `std::runtime_error`, callback B that increments a counter; `REQUIRE_THROWS_AS(cts.cancel(), std::runtime_error)`; REQUIRE counter equals 1 and `is_cancellation_requested()` true.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
