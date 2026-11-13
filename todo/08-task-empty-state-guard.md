# 08 — Default-constructed Task dereferences a null state (UB / crash)

**Severity:** High  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/task.hpp`
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/task.hpp` `Task::await_ready` (lines 45-47 returns true for empty task), `await_suspend` (lines 55-60), `await_resume` (lines 67-78, `state_->exception` line 68), `wait_until_ready` (lines 125-130, `state_->is_ready()` line 126).

## Problem Description
1. `Task() noexcept = default` (line 26) leaves `state_` null. `await_ready()` says an empty task is ready, so `co_await Task<int>{}` goes straight to `await_resume()`, which dereferences `state_` (line 68): null pointer dereference.
2. `result()` / `wait()` on an empty task call `wait_until_ready()` which dereferences `state_` (line 126). UBSan reports `member access within null pointer` and the process segfaults (probe confirmed).
3. Same for moved-from tasks (`Task(Task&&)` leaves `state_` null).

**Audit evidence:** UBSan/ASan probe: `Task<int> t; t.result();` -> null deref / SEGV at task_state.hpp:26.

## Step-by-Step Instructions
1. Add a private helper in `Task` that throws `std::logic_error` with a message like "Task has no shared state (default-constructed or moved-from)." when `state_` is null. (include `<stdexcept>`)
2. Call it at the top of `await_resume` and `wait_until_ready`.
3. In `await_suspend` return `awaiting` immediately when `state_` is null (so the awaiting coroutine resumes and `await_resume` throws).
4. Leave `await_ready` returning true for the empty task (existing test `Task default construction...` relies on it).
5. Update doc comments with `@throws std::logic_error`.

## Unit Test & Acceptance Criteria
- In `tests/test_task.cpp` add TEST_CASE "Empty and moved-from tasks throw logic_error instead of crashing" tagged `[task][empty]`: REQUIRE_THROWS_AS `Task<int>{}.result()` and `Task<void>{}.wait()` throw `std::logic_error`; create a ready task, move it into another variable, then REQUIRE_THROWS_AS the moved-from one's `.result()` throws `std::logic_error`; inside a coroutine `co_await Task<int>{}` inside try/catch records that `std::logic_error` was caught and REQUIRE it.
- Existing test "Task default construction and already-completed fast paths" still passes.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
