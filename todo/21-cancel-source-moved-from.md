# 21 — Moved-from CancellationTokenSource dereferences null (cancel/token/cancel_after)

**Severity:** Medium  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `src/cancellation.cpp`
- `tests/test_cancellation.cpp`

## Exact Location
- `src/cancellation.cpp` `CancellationTokenSource::token` (67-69), `cancel` (71-73), `cancel_after` (75-94, `auto state = state_;` line 76).

## Problem Description
1. The class is implicitly movable. After `CancellationTokenSource b(std::move(a));` `a.state_` is null; `a.cancel()` calls `state_->cancel()` (line 72) → null dereference (UBSan + SEGV in probe). `token()` returns a token with null state (works as a never-cancel token, silently).
2. `cancel_after` on a moved-from source starts a timer that dereferences null when it fires (line 91).

**Audit evidence:** Probe: `runtime error: member call on null pointer` then SEGV at cancellation.cpp:13.

## Step-by-Step Instructions
1. In `cancel()` and `cancel_after()` return immediately when `state_` is null.
2. In `token()` keep returning a default (never-cancelable) token for a null state; add a comment.
3. Do not change the header.

## Unit Test & Acceptance Criteria
- In `tests/test_cancellation.cpp` add TEST_CASE "Moved-from CancellationTokenSource is inert" tagged `[cancellation][move]`: move-construct b from a; `REQUIRE_NOTHROW(a.cancel())`, `REQUIRE_NOTHROW(a.cancel_after(1ms))`; `REQUIRE_FALSE(a.token().is_cancellation_requested())`; `b.cancel()` still cancels b's tokens.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
