# 26 — test_thread_pool.cpp destructor tests do not test what they claim; unsafe thread_local juggling; REQUIRE on worker threads

**Severity:** Medium  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `tests/test_thread_pool.cpp`

## Exact Location
- `tests/test_thread_pool.cpp` lines 88-99 ("destructor cleanly stops timer thread with pending timers": `temp_pool` is never used, the coroutine runs on the GLOBAL pool), lines 101-132 (raw `detail::current_pool = ...` assignments, restored only on the success path), lines 58-73 and 75-86 (`REQUIRE` inside a std::thread).

## Problem Description
1. Test at line 88 creates `ThreadPool temp_pool(1)` but `detail::current_pool` is null on the main thread, so the 5000 ms sleep goes to the global pool; the destructor of `temp_pool` is not exercised with a pending timer.
2. If a REQUIRE throws between `current_pool = &temp_pool` and the reset, the thread_local keeps a dangling pointer for later tests.
3. Catch2 assertions on non-main threads are not generally thread safe (line 67).

## Step-by-Step Instructions
1. Add a local RAII helper in the anonymous namespace that sets `detail::current_pool` in its constructor and restores the previous value in its destructor; use it in all three destructor tests.
2. In the test at line 88 start the sleeping coroutine under that helper so it really lands in `temp_pool`.
3. In the two `std::thread` tests store results into atomics/plain variables inside the thread and perform every REQUIRE after `join()` on the main thread.
4. Replace the fixed `sleep_for` waits (lines 45, 96, 111, 129) by a polling wait (up to 2 s) where a condition exists, otherwise keep the sleep with a comment.

## Unit Test & Acceptance Criteria
- All tests pass; run under ASan/TSan; `current_pool` is null after each test case.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
