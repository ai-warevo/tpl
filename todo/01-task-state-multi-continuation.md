# 01 — Support multiple awaiters on one Task (single continuation slot loses waiters)

**Severity:** Critical  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/task_state.hpp`
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/task_state.hpp` struct `TaskStateBase`: member `continuation` (line 17), `try_attach_continuation` (lines 35-42), `complete` (lines 47-58).
- `include/aw/tpl/threading/tasks/task.hpp` `Task::await_suspend` (lines 55-60) is the only caller; do NOT edit it in this task.

## Problem Description
1. `Task<T>` is copyable (task.hpp lines 35-36), so several coroutines can `co_await` copies of the same task. `when_any` also attaches a hidden watcher to every task it receives (when_any.hpp lines 35-58).
2. `TaskStateBase` stores exactly ONE `std::coroutine_handle<> continuation` (line 17). `try_attach_continuation` (line 40) overwrites it unconditionally.
3. Sequence: awaiter A attaches, awaiter B attaches and overwrites A. When the task completes, `complete()` resumes only B. Coroutine A is never resumed: its frame leaks and anything waiting on A hangs forever.
4. The same lost-wakeup happens if a user awaits a task that was also passed to `when_any`: either the user's coroutine or the watcher is silently dropped.

**Audit evidence:** Probe (outside repo): two consumers awaiting one slow Task<int>: one consumer finished, the other stayed not-ready forever (output `a ready=0 b ready=1`).

## Step-by-Step Instructions
1. Replace the single `continuation` member by a `std::vector<std::coroutine_handle<>>` named `continuations` (add `#include <vector>`).
2. In `try_attach_continuation`, keep the existing lock and the existing `ready` check that returns false. Instead of assigning, append `awaiting` to `continuations` and return true.
3. In `complete()`, under the existing lock set `ready = true` and move/swap the whole vector into a local vector, leaving the member empty. Release `done_sem` exactly as before. Then resume every handle of the local vector, in the order they were attached, one after another on the current thread.
4. Do not change any other behaviour and do not rename `complete`, `is_ready` or `try_attach_continuation`; other files depend on those names.
5. Search the repository for any other use of the old `continuation` member (use grep). There should be none outside task_state.hpp; if you find one, stop and report instead of editing a third file.

## Unit Test & Acceptance Criteria
- Add TEST_CASE "Multiple coroutines can await the same Task" tagged `[task][continuation]` to `tests/test_task.cpp`: create a `Task<int>` that sleeps 80 ms (`async_sleep`) and returns 7. Create three consumer coroutines (a lambda taking `Task<int>` by value and returning `co_await` of it plus nothing else), each given a COPY of the slow task. Store the three consumer tasks, then call `.result()` on each from the main thread. REQUIRE each equals 7. Before the fix this hangs (use an external 30 s timeout when verifying the failure).
- Add TEST_CASE "TaskState keeps every attached continuation" tagged `[task][continuation]`: construct a `TaskState<int>` directly, call `try_attach_continuation(std::noop_coroutine())` twice and REQUIRE both calls return true; call `complete()`; REQUIRE `is_ready()` is true; REQUIRE a third `try_attach_continuation` call returns false.
- Existing tests in test_task.cpp, test_when_all.cpp and test_when_any.cpp keep passing.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
