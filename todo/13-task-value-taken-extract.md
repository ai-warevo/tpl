# 13 — Throw TaskAlreadyMovedException when a move-only result is extracted twice

**Severity:** High  
**Depends on:** 12 (`task-value-taken-state`), 11 (`task-move-only-container-copy`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/task.hpp`
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/task.hpp` `Task::await_resume` move branch (lines 74-76).

## Problem Description
1. See task 12 (`task-value-taken-state`): the second extraction returns a moved-from value silently; concurrent extraction is a data race.
2. A Task<std::unique_ptr<T>> can be awaited from several copies of the Task (shared state) but only one consumer may get the pointer.

## Step-by-Step Instructions
1. In the non-copyable branch of `await_resume` (the branch selected when the type is not truly copyable) first call `state_->try_take()`; if it returns false, `throw TaskAlreadyMovedException{}` (header task_exceptions.hpp is already included). Only if true, return `std::move(*state_->value)`.
2. The stored-exception check at the top of `await_resume` must stay first so a failed task still reports its own exception.
3. Document `@throws TaskAlreadyMovedException` on `await_resume`, `result()`.

## Unit Test & Acceptance Criteria
- In `tests/test_task.cpp` add TEST_CASE "Second result() of a move-only task throws TaskAlreadyMovedException" tagged `[task][move]`: first `result()` returns non-null, second `REQUIRE_THROWS_AS(..., TaskAlreadyMovedException)`.
- Add TEST_CASE "Concurrent extraction of a move-only result yields exactly one value" tagged `[task][move][race]`: copy a ready `Task<std::unique_ptr<int>>` into 8 `std::thread`s each calling `result()` inside try/catch; count successes and `TaskAlreadyMovedException`s with atomics; after joining REQUIRE exactly 1 success and 7 exceptions. (No REQUIRE inside the threads.)
- Existing move-only test still passes.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
