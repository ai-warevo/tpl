# 03 — Complete the task from final_suspend (after locals are destroyed) using symmetric transfer

**Severity:** Critical  
**Depends on:** 02 (`task-state-finish-api`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/promise.hpp`
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/promise.hpp` `PromiseBase::final_suspend` (line 26), `PromiseBase::unhandled_exception` (lines 31-34), `Promise<T>::return_value` (lines 41-45), `Promise<void>::return_void` (lines 52-54).

## Problem Description
1. `final_suspend()` returns `std::suspend_never` (line 26) and the waiter is released earlier, inside `return_value` / `return_void` / `unhandled_exception` via `state->complete()`.
2. At that moment the producer coroutine has NOT yet destroyed its local variables. A thread blocked in `Task::wait()` / `result()` wakes through `done_sem` and may destroy objects that those still-alive locals refer to (for example a lock guard, or a reference parameter pointing at the caller's stack).
3. The awaiting coroutine is also resumed inline from inside the producer's body (nested resume), so `Task` chains of depth N consume N stack frames and overflow for large N.
4. Exceptions thrown by a `co_return` expression's value conversion reach `unhandled_exception`, which also completes early.

**Audit evidence:** Probe: after `t.wait()` returned the flag set by a local's destructor was still 0; a prototype of this exact design printed 1 under -O0/-O2 with and without ASan. Baseline -O2 400000-deep chain crashed with no output; prototype returned 400000.

## Step-by-Step Instructions
1. Inside `PromiseBase`, add a nested awaiter type used by `final_suspend`. It holds a copy of the `std::shared_ptr<TaskState<T>>`. `await_ready` returns false. `await_suspend(std::coroutine_handle<> h)` must: (a) move the shared_ptr into a local variable, (b) call `h.destroy()` to destroy the coroutine frame (this destroys all locals first), (c) call `finish()` on the local state and return the resulting handle (symmetric transfer). `await_resume` does nothing. All of these are `noexcept`.
2. Change `final_suspend()` to return an instance of that awaiter constructed with a copy of `state`, keeping `noexcept`.
3. Remove the `state->complete()` calls from `unhandled_exception`, `Promise<T>::return_value` and `Promise<void>::return_void`. `unhandled_exception` must still store `std::current_exception()` into `state->exception`; the `return_*` functions must still store the value.
4. Because the awaiter destroys the frame itself, `final_suspend` must never be skipped: confirm by reading that every path (normal return, `return_void`, exception) reaches `final_suspend`.
5. Add a short comment explaining that the state outlives the frame because the awaiter holds its own shared_ptr.

## Unit Test & Acceptance Criteria
- In `tests/test_task.cpp` add TEST_CASE "Waiter is released only after coroutine locals are destroyed" tagged `[task][final_suspend]`: define a local RAII struct whose destructor sleeps 50 ms and then stores true in a `std::atomic<bool>` passed by reference. Create a `Task<void>` coroutine (parameter: the atomic by reference) that constructs the RAII object and does `co_return`. Call `.wait()` from the main thread and then REQUIRE the atomic is true. (Fails before the fix: probe printed `destroyed=0`.)
- Add TEST_CASE "Long chains of awaiting tasks do not overflow the stack" tagged `[task][chain]`: a static recursive coroutine returning `Task<int>` that returns 0 for n==0 and `1 + co_await f(n-1)` otherwise. Always run depth 2000 and REQUIRE the result equals 2000. Additionally, guard a depth-200000 run with the preprocessor macro `NDEBUG` and do not compile it under sanitizers (use `__SANITIZE_ADDRESS__` / `__has_feature(address_sanitizer)` checks): GCC only guarantees tail calls with optimisation. In Release (`-O2`) the 400000-deep chain crashed before the fix and succeeded after (verified in a prototype).
- Existing exception tests (`Task<void> and Task<T> propagate exceptions properly`) still pass.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
