# 09 — TaskState<T> requires default-constructible, assignable T (store the result in std::optional<T>)

**Severity:** High  
**Depends on:** 03 (`promise-final-awaiter`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/task_state.hpp`
- `include/aw/tpl/threading/tasks/task.hpp`

## Exact Location
- `include/aw/tpl/threading/tasks/task_state.hpp` `TaskState<T>::value` (line 68 `T value{};`).
- `include/aw/tpl/threading/tasks/task.hpp` `Task::await_resume` value branch (lines 71-77).

## Problem Description
1. `T value{}` forces every `Task<T>` result type to be default-constructible. `Task<ND>` for a type with only a converting constructor fails to compile (probe: `could not convert '<brace-enclosed initializer list>()' ... to 'ND'`).
2. It also constructs a throw-away T for every task even when the coroutine throws.

## Step-by-Step Instructions
1. In `TaskState<T>` replace the `T value{}` member by `std::optional<T> value` (include `<optional>`).
2. In `Task::await_resume` adapt both existing branches: the copy branch returns `*state_->value` (copy), the move branch returns `std::move(*state_->value)`. Keep the `is_copy_constructible_v` selection exactly as it is for now (task 11 (`task-move-only-container-copy`) improves it).
3. Do NOT edit promise.hpp in this task: its `this->state->value = std::forward<U>(v)` line (promise.hpp:43) keeps compiling because `std::optional` is assignable from `U`. (task 10 (`promise-return-by-value`) replaces it.)
4. Search for any other direct use of `->value` (grep) and report instead of editing a third file if you find one.

## Unit Test & Acceptance Criteria
- No new test file. Acceptance: the whole existing suite passes unchanged, including `Task supports move-only return types like std::unique_ptr` and the `when_all` tests.
- In `tests/test_task.cpp` add TEST_CASE "Task result type does not need to be default constructible" tagged `[task][value]` only AFTER task 10 (`promise-return-by-value`) is done; for this task skip adding it.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
