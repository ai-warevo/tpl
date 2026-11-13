# 07 — when_any: winner_index written after `completed` is published (data race / stale index)

**Severity:** High  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `include/aw/tpl/when_any.hpp`
- `tests/test_when_any.cpp`

## Exact Location
- `include/aw/tpl/when_any.hpp` `spawn_when_any_watcher` (lines 36-58, especially line 47 `completed.exchange`, line 48 `winner_index = index`, lines 50-53), `WhenAnyAwaiter::await_ready` (lines 63-65), `await_resume` (lines 76-78).

## Problem Description
1. The winning watcher first publishes `completed = true` with an exchange (line 47) and only afterwards writes `winner_index` (line 48), a plain non-atomic `size_t`.
2. A concurrent `WhenAnyAwaiter::await_ready()` (acquire load, line 64) can already observe `completed == true` and return true; `await_resume()` then reads `winner_index` (line 77) while the watcher is still about to write it: a data race (UB) and a possible stale index 0.
3. On the `await_suspend` path the waiter also reads `winner_index` without synchronising with the write (the write is outside the lock).

**Audit evidence:** Code-reading finding: TSan could not run in this sandbox (unexpected memory mapping), so this race is established by analysis, not by an observed report.

## Step-by-Step Instructions
1. Restructure the winner claim in `spawn_when_any_watcher` so the index is written BEFORE it becomes visible: take `state->mtx`; if `completed` is still false, store `winner_index = index`, set `completed` to true (keep it a `std::atomic<bool>`, store with release), copy `waiter` to a local handle; then release the lock.
2. Only after the lock is released, if the local waiter handle is non-null, call `schedule_on_pool` on it. Losers (completed already true) do nothing.
3. Keep `WhenAnyAwaiter` logic unchanged except: in `await_suspend` it already locks `mtx` and re-checks `completed`; verify that with the new ordering reading `winner_index` after observing `completed` (acquire) is race free and add a comment saying so.
4. Do not change public function signatures.

## Unit Test & Acceptance Criteria
- In `tests/test_when_any.cpp` add TEST_CASE "when_any reports the correct index under immediate completion races" tagged `[when_any][race]`: loop 2000 times; each iteration calls the variadic `when_any` with two `Task<int>` arguments: the first coroutine sleeps 1 ms via `async_sleep` and then returns 100, the second returns 200 immediately. Call `.result()` and REQUIRE the returned index equals 1 in every iteration. If this proves flaky on very slow machines, relax it to: index is 0 or 1, and when it is 0 the first task reports `await_ready()` true.
- Run the test binary under ThreadSanitizer when available; no data-race report on `winner_index`.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
