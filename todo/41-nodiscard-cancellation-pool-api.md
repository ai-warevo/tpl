# 41 — Add [[nodiscard]] and noexcept where appropriate on cancellation/pool API

**Severity:** Low  
**Depends on:** 24 (`cancel-sleep-unregister-on-resume`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/cancellation.hpp`
- `include/aw/tpl/thread_pool.hpp`

## Exact Location
- `cancellation.hpp`: `none()` line 63, `is_cancellation_requested` 69, `token()` 104, `async_sleep` 141; `thread_pool.hpp`: `thread_count` 76, `async_sleep` lines 37/81/139; unused member `resumed` (line 128) after the sleep-awaiter task.

## Problem Description
1. Pure query functions silently allow ignoring results (`async_sleep(100ms);` without co_await compiles and does nothing). `CancelableSleepAwaiter` still has an unused `resumed` member after the lifetime fix.

## Step-by-Step Instructions
1. Add `[[nodiscard]]` to the listed functions. For `async_sleep` the attribute must appear consistently on the forward declaration (thread_pool.hpp:37), the friend declaration (81) and the definition (139).
2. Remove the unused `resumed` data member of `CancelableSleepAwaiter` once no code refers to it (grep first).
3. Do not change signatures otherwise.

## Unit Test & Acceptance Criteria
- Library and tests compile with `-Wall -Wextra -Werror`; the suite passes.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
