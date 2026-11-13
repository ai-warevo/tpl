# 24 — async_sleep(token): remove the cancel callback when the timer wins

**Severity:** Medium  
**Depends on:** 23 (`cancel-register-id-tests`)

## Target Files (ONLY these may be edited)
- `src/cancellation.cpp`
- `tests/test_cancellation.cpp`

## Exact Location
- `src/cancellation.cpp` `CancelableSleepAwaiter::await_suspend` (lines 101-125).

## Problem Description
1. When the timer expires first the callback stays in the token's list until the token is cancelled or destroyed (memory growth, see register-id task). When cancel wins, the timer still sits in the pool's timer heap until its deadline (cannot be removed; accepted).

## Step-by-Step Instructions
1. After the register-id API exists, store the id returned by `register_callback` in a shared control block (the same shared_ptr that holds the `resumed` flag; define a small local struct with `std::atomic<bool> resumed` and `std::atomic<std::uint64_t> callback_id`).
2. In the timer bridge, after its `exchange` on the flag wins (timer path), call `unregister_callback` with the stored id on the local token copy.
3. Handle the race where the callback was registered AFTER the timer already won: register callback returns an id; if the flag is already set, unregister it immediately.
4. Keep exactly-once resume semantics.

## Unit Test & Acceptance Criteria
- In `tests/test_cancellation.cpp` add TEST_CASE "Sleeping repeatedly on one token does not accumulate callbacks" tagged `[cancellation][leak]`: one source; coroutine performing 200 sequential `async_sleep(1ms, token)`; after `result()` REQUIRE `token.registered_callback_count() == 0`.
- Existing cancel tests pass.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
