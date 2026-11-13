# 12 — Add a one-shot `value_taken` guard to TaskState (state support for single-use move-only extraction)

**Severity:** High  
**Depends on:** 09 (`task-state-optional-value`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/task_state.hpp`
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/task_state.hpp` `TaskState<T>` (lines 66-69).

## Problem Description
1. Move-only results are moved out of the shared state in `Task::await_resume` (task.hpp:75). Two awaiters or a second `result()` call get a moved-from (null) value silently: probe `auto p1 = t.result(); auto p2 = t.result();` returned p1 set, p2 null, no error.
2. Two threads calling `result()` concurrently race on the same `unique_ptr` (data race).
3. `TaskAlreadyMovedException` (task_exceptions.hpp) exists but is never thrown.

## Step-by-Step Instructions
1. Add to the primary `TaskState<T>` a public `std::atomic<bool> value_taken{false}` member (include `<atomic>`), and a member function `bool try_take()` that returns `!value_taken.exchange(true, std::memory_order_acq_rel)`.
2. Do not modify `TaskState<void>`. Do not use it yet (task 13 (`task-value-taken-extract`)).

## Unit Test & Acceptance Criteria
- In `tests/test_task.cpp` add TEST_CASE "TaskState::try_take is one-shot" tagged `[task][state]`: construct `TaskState<int>`, REQUIRE first `try_take()` true, second false.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
