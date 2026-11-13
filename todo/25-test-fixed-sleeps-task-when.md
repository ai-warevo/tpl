# 25 — Replace fixed sleep_for waits in test_task.cpp and test_when_all.cpp (flaky on loaded CI)

**Severity:** Medium  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `tests/test_task.cpp`
- `tests/test_when_all.cpp`

## Exact Location
- `tests/test_task.cpp` line 21 (`sleep_for(30ms)`), `tests/test_when_all.cpp` line 81 (`sleep_for(25ms)`).

## Problem Description
1. The tests assume a worker finishes a trivial coroutine within 25-30 ms. On a loaded runner it may not, so `await_ready()` is false and the "fast path" assertion (line 22) fails spuriously.

## Step-by-Step Instructions
1. In each file add a small local helper in an anonymous namespace that polls a predicate (`std::function<bool()>`) every 1 ms until a 5 s deadline and returns whether it became true.
2. Replace the fixed sleeps by that helper polling `await_ready()` of the relevant tasks; REQUIRE the helper returned true.
3. Do NOT change test_when_any.cpp here (it is covered with the cancellation test cleanup if needed).

## Unit Test & Acceptance Criteria
- Both files still pass; run each test 50 times in a loop (`--rng-seed` varied or `ctest --repeat until-fail:50`) with no failure.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
