# 15 — CancelableSleepAwaiter::await_suspend touches `this` after the coroutine may have resumed; awaiter cannot be reused

**Severity:** High  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `src/cancellation.cpp`
- `tests/test_cancellation.cpp`

## Exact Location
- `src/cancellation.cpp` `CancelableSleepAwaiter::await_suspend` (lines 101-125): `token.register_callback(try_resume)` line 108, then `duration` read at line 124. `include/aw/tpl/cancellation.hpp` member `resumed` (line 128).

## Problem Description
1. `await_suspend` registers the cancel callback FIRST (line 108). If the token is cancelled concurrently (or already) the callback calls `schedule_on_pool(h)` and another worker resumes and may destroy the coroutine frame — which owns the awaiter (`this`) — while the original thread is still in `await_suspend` and then reads `duration` (line 124): use-after-free.
2. The `resumed` flag is a member created once with the awaiter (hpp line 128). If the same awaiter object is co_awaited twice, the second wait finds `resumed == true`, the timer's `exchange` returns true and the coroutine is never resumed: hang.
3. `await_suspend` is `const` and the callback captures the handle; nothing after registration may use members.

## Step-by-Step Instructions
1. At the top of `await_suspend` copy `duration`, `token` and create a NEW `std::shared_ptr<std::atomic<bool>>` into local variables. Never use `this` / members after that point.
2. Build `try_resume` from those locals only (new flag + handle).
3. Start the timer bridge FIRST using the local duration and `try_resume`; THEN register the cancel callback with the local token copy. (If registration runs the callback inline because the token is already cancelled, the shared flag guarantees a single resume.)
4. Do not edit the header (the now-unused `resumed` member is removed in task 41 (`nodiscard-cancellation-pool-api`)).

## Unit Test & Acceptance Criteria
- In `tests/test_cancellation.cpp` add TEST_CASE "The same sleep awaiter can be awaited twice" tagged `[cancellation][sleep]`: inside a coroutine create `auto s = async_sleep(10ms, token)` (token from a never-cancelled source) and `co_await s;` twice, then return 1; poll the task with a 2 s deadline (`await_ready()` + `sleep_for(5ms)`) and REQUIRE it became ready and `result() == 1`.
- Add TEST_CASE "Concurrent cancel while suspending does not touch a destroyed awaiter" tagged `[cancellation][race]`: loop 500 iterations; each: fresh source, start a coroutine doing `co_await async_sleep(1000ms, token)` inside try/catch(OperationCanceledException) returning 1, immediately call `cts.cancel()` from the main thread, `result()` must be 1. Run under ASan: no report.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
