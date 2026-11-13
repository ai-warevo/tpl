# 16 — when_all(vector): first failure is rethrown while sibling tasks are still running

**Severity:** High  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `include/aw/tpl/when_all.hpp`
- `tests/test_when_all.cpp`

## Exact Location
- `include/aw/tpl/when_all.hpp` vector overload lambdas (lines 39-44 for void, 46-53 for value): a range-for loop that co_awaits each task of the moved-in vector in order.

## Problem Description
1. `co_await t` rethrows as soon as task i fails. Tasks i+1.. are never awaited: they keep running after `when_all` already failed, and their exceptions are never observed.
2. Any state those tasks reference on the caller's stack (reference parameters) may be destroyed once the caller sees the exception → use-after-free. C# `Task.WhenAll` waits for all tasks first.

## Step-by-Step Instructions
1. In both lambdas await EVERY task inside its own try/catch; remember the first caught `std::exception_ptr` (by index order) in a local variable and keep going.
2. For the value version store results only for successful tasks; after the loop, if an exception was remembered, `std::rethrow_exception` it; otherwise return the results as today.
3. Keep empty-vector behaviour (returns empty vector / completes).

## Unit Test & Acceptance Criteria
- In `tests/test_when_all.cpp` add TEST_CASE "when_all waits for every child before failing" tagged `[when_all][exceptions]`: vector<Task<int>> with task0 throwing `std::runtime_error` immediately and task1 sleeping 60 ms then incrementing a shared `std::atomic<int>` (passed by reference) and returning 1. `REQUIRE_THROWS_AS(when_all(...).result(), std::runtime_error)` then immediately REQUIRE the counter equals 1.
- Same scenario for `vector<Task<void>>`.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
