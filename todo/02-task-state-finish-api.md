# 02 — Add TaskStateBase::finish() that completes the state and returns the continuation instead of resuming it

**Severity:** Critical  
**Depends on:** 01 (`task-state-multi-continuation`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/task_state.hpp`
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/task_state.hpp` `TaskStateBase::complete` (lines 47-58 before task 01 (`task-state-multi-continuation`); after task 01 (`task-state-multi-continuation`) it holds the vector swap).
- Needed by `include/aw/tpl/threading/tasks/promise.hpp` `PromiseBase::final_suspend` (line 26) in the next task.

## Problem Description
1. `complete()` resumes the awaiting coroutine with a plain `cont.resume()` (line 56) from inside `Promise::return_value` / `return_void` / `unhandled_exception`, i.e. while the producer coroutine is still running its own body.
2. Because it is a nested resume, a chain of N awaiting tasks nests N resumes on one thread stack (stack overflow for long chains) and C++20 symmetric transfer is never used.
3. The state needs a variant of completion that does everything except resuming, so that the promise can return the handle from `final_suspend` (done in task 03 (`promise-final-awaiter`)).

## Step-by-Step Instructions
1. Add a new public member function `finish()` to `TaskStateBase` returning `std::coroutine_handle<>`.
2. `finish()` must: lock `mtx`, set `ready = true`, swap the continuations vector into a local, unlock; call `done_sem.release()`; then, if the local vector is empty return `std::noop_coroutine()`.
3. If the local vector is non-empty: for every handle EXCEPT the first, hand it to the pool with `::aw::tpl::detail::schedule_on_pool(handle)` (add `#include <aw/tpl/thread_pool.hpp>`); then return the first handle. (Extra waiters run on the pool, the first one is resumed by symmetric transfer.)
4. Rewrite `complete()` to call `finish()` and then call `.resume()` on the returned handle. Keeping `complete()` means promise.hpp still compiles unchanged until task 03 (`promise-final-awaiter`).
5. Update the doc comment of both functions to describe the new behaviour. Do not touch any other file.

## Unit Test & Acceptance Criteria
- In `tests/test_task.cpp` add TEST_CASE "TaskState::finish returns noop when nobody waits" tagged `[task][state]`: construct `TaskState<void>`, call `finish()`; REQUIRE the returned handle equals `std::noop_coroutine()`; REQUIRE `is_ready()` true; REQUIRE `done_sem.try_acquire()` returns true.
- Add TEST_CASE "TaskState::finish hands back the first continuation" tagged `[task][state]`: attach a `std::noop_coroutine()` handle with `try_attach_continuation`, call `finish()` and REQUIRE the returned handle equals that same handle value.
- Add TEST_CASE "Multiple awaiters all resume after complete" reusing the 3-consumer scenario from task 01 (`task-state-multi-continuation`) (if that test already exists, skip this one).
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
