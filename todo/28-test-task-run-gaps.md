# 28 — Coverage blind spots for Task<>::run

**Severity:** Medium  
**Depends on:** 14 (`task-blocking-wait-on-worker`)

## Target Files (ONLY these may be edited)
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/task.hpp` `Task::run` (lines 109-119), tests only cover the throwing int lambda.

## Problem Description
1. No test covers: void-returning callable, capturing-by-value lambda whose original closure is destroyed before the task runs, move-only callable, `run` called from a worker thread, large results.

## Step-by-Step Instructions
1. Append TEST_CASEs (tag `[task][run]`): (1) `Task<>::run` of a void lambda that sets an atomic flag passed through a shared_ptr captured by value → `.wait()` then flag true; (2) lambda capturing a `std::string` by value created in an inner scope that ends before `.result()` is called → result equals the string; (3) move-only functor holding a `std::unique_ptr<int>` and returning its value; (4) nested `Task<>::run` from inside a coroutine using `co_await` (not `.result()`).

## Unit Test & Acceptance Criteria
- All four pass under ASan.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
