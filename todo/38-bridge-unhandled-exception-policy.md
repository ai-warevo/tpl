# 38 — Silent `unhandled_exception() {}` in fire-and-forget bridge coroutines can hang waiters

**Severity:** Low  
**Depends on:** 20 (`cancel-callback-exception-isolation`)

## Target Files (ONLY these may be edited)
- `src/cancellation.cpp`
- `include/aw/tpl/when_any.hpp`

## Exact Location
- `src/cancellation.cpp` lines 84 and 116 (`void unhandled_exception() noexcept {}` in `CancelTimerBridge` and `TimerBridge`), line 91 (`st->cancel()`); `include/aw/tpl/when_any.hpp` line 31 (`WhenAnyWatcher::promise_type`). All three are marked `LCOV_EXCL_LINE`.

## Problem Description
1. Bridge coroutines swallow any exception. The only throw sources (`schedule_on_pool` → bad_alloc/system_error, callback exceptions in `st->cancel()`) leave a waiter that will never be resumed (silent hang).

## Step-by-Step Instructions
1. Change each `unhandled_exception` to call `std::terminate()` (include `<exception>`), keep the LCOV exclusion comment, and add a comment explaining that failing fast is preferable to a silent hang.
2. Note: callback exceptions from `st->cancel()` in `cancel_after`'s bridge now terminate, so wrap that call (cancellation.cpp line 91) in a try/catch(...) that ignores errors.

## Unit Test & Acceptance Criteria
- No death test possible; acceptance: suite passes, `grep -n "unhandled_exception() noexcept {}"` returns no results in these two files.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
