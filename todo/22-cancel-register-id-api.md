# 22 — Add registration ids and unregister_callback to CancellationToken (callbacks accumulate forever)

**Severity:** Medium  
**Depends on:** 20 (`cancel-callback-exception-isolation`), 21 (`cancel-source-moved-from`), 15 (`cancel-sleep-awaiter-lifetime`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/cancellation.hpp`
- `src/cancellation.cpp`

## Exact Location
- `include/aw/tpl/cancellation.hpp` `CancellationState` (lines 31-46), `CancellationToken::register_callback` (line 81). `src/cancellation.cpp` lines 10-37, 55-59.

## Problem Description
1. Every `async_sleep(d, token)` pushes a `std::function` (capturing the coroutine handle and a shared_ptr) into `callbacks` (cancellation.cpp:31). Nothing removes it until `cancel()`. A long-lived token reused by a loop of N sleeps grows by N entries: unbounded memory growth.
2. There is no way for a client to unregister a callback.

## Step-by-Step Instructions
1. In `CancellationState` change `callbacks` to a vector of pairs (`std::uint64_t` id, `std::function<void()>`) and add a `std::uint64_t next_id` counter (guarded by `mtx`).
2. Change `register_callback` of the state AND of `CancellationToken` to return `std::uint64_t`: the new id (> 0), or 0 when the callback was run immediately (already cancelled) or the token has no state.
3. Add `bool unregister_callback(std::uint64_t id)` to both: removes the entry under the lock and returns true if found.
4. Add `std::size_t registered_callback_count() const` to `CancellationToken` (diagnostic, locks the mutex, returns 0 for a state-less token).
5. Add `[[nodiscard]]` to the new return values only where ignoring is clearly a bug (`unregister_callback`, `registered_callback_count`); the id return of `register_callback` must NOT be nodiscard (existing callers ignore it).
6. Existing callers (`cancellation.cpp` awaiter, tests) must still compile unchanged.

## Unit Test & Acceptance Criteria
- Tests are in the next task (`cancel-register-id-tests`). Acceptance here: the whole existing suite still compiles and passes.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
