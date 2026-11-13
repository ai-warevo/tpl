# 30 — WhenAnyAwaiter::await_suspend is `noexcept` but takes a mutex (can throw → std::terminate)

**Severity:** Medium  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `include/aw/tpl/when_any.hpp`
- `tests/test_when_any.cpp`

## Exact Location
- `include/aw/tpl/when_any.hpp` `WhenAnyAwaiter::await_suspend` (lines 67-74), `await_ready` (63-65), `await_resume` (76-78).

## Problem Description
1. `await_suspend` is declared `noexcept` (line 67) but constructs a `std::lock_guard` (line 68); `std::mutex::lock` may throw `std::system_error`, which then calls `std::terminate`.

## Step-by-Step Instructions
1. Remove `noexcept` from `WhenAnyAwaiter::await_suspend` only.
2. Do not touch `await_ready` or `await_resume`.

## Unit Test & Acceptance Criteria
- In `tests/test_when_any.cpp` add TEST_CASE "WhenAnyAwaiter::await_suspend is not noexcept" tagged `[when_any][noexcept]` containing a `static_assert` that `noexcept(std::declval<detail::WhenAnyAwaiter&>().await_suspend(std::noop_coroutine()))` is false. Existing tests pass.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
