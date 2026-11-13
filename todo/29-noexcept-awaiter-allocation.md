# 29 — Task::await_suspend is `noexcept` but can throw (lock/allocation) → std::terminate

**Severity:** Medium  
**Depends on:** 01 (`task-state-multi-continuation`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/task.hpp`
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/task.hpp` `Task::await_suspend` (line 55), which calls `try_attach_continuation` (task_state.hpp lines 35-42; it locks a mutex and, after task 01 (`task-state-multi-continuation`), appends to a vector).

## Problem Description
1. After the vector change `try_attach_continuation` can throw `std::bad_alloc` (push_back) or `std::system_error` (mutex). Because the function is `noexcept`, `std::terminate` is called instead of an exception reaching the awaiting coroutine.

## Step-by-Step Instructions
1. Remove `noexcept` from `Task::await_suspend`; let exceptions propagate to the awaiting coroutine.
2. Leave other awaiter functions' attributes as they are.

## Unit Test & Acceptance Criteria
- In `tests/test_task.cpp` add TEST_CASE "Task::await_suspend is not noexcept" tagged `[task][noexcept]` containing a `static_assert` that `noexcept(std::declval<Task<int>&>().await_suspend(std::noop_coroutine()))` is false. Existing tests pass.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
