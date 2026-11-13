# 27 — REQUIRE/assert macros executed inside coroutines on pool worker threads

**Severity:** Medium  
**Depends on:** 25 (`test-fixed-sleeps-task-when`)

## Target Files (ONLY these may be edited)
- `tests/test_when_all.cpp`

## Exact Location
- `tests/test_when_all.cpp` lines 22-24 (REQUIRE inside the `runner` coroutine).

## Problem Description
1. The `runner` coroutine runs on a pool worker thread; Catch2 assertion macros are not guaranteed thread safe there (unless the experimental thread-safe mode is enabled). A failure throws inside the coroutine, which is captured into the Task and rethrown by `.wait()`, hiding the real assertion message and context.

## Step-by-Step Instructions
1. Make the coroutine return the three values (use `Task<std::tuple<int, std::string, bool>>` or similar) and move all REQUIREs to the main thread after `.result()`.

## Unit Test & Acceptance Criteria
- The test passes; deliberately changing an expected value makes the failure be reported by Catch2 with the correct location (verify manually, then revert).
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
