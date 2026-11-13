# 33 — default_thread_count_ is a plain static written and read from different threads

**Severity:** Low  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `include/aw/tpl/thread_pool.hpp`
- `tests/test_thread_pool.cpp`

## Exact Location
- `include/aw/tpl/thread_pool.hpp` line 121 (`static inline size_t default_thread_count_`), used as default argument at line 62.
- `src/thread_pool.cpp` lines 14-18 (write) and 21 (read) keep compiling unchanged with an atomic, so `src/thread_pool.cpp` is NOT a target of this task.

## Problem Description
1. `set_default_threads` writes the static while another thread may read it (global() or the constructor default argument): data race. It also silently has no effect once the global pool exists; this is undocumented.

## Step-by-Step Instructions
1. Change the declaration to `std::atomic<size_t>` initialised with `std::thread::hardware_concurrency()` (use a helper to turn 0 into 2) and include `<atomic>`.
2. Document in the doc comment of `set_default_threads` that it has no effect after the global pool was created.

## Unit Test & Acceptance Criteria
- In `tests/test_thread_pool.cpp` add TEST_CASE "set_default_threads after global pool creation is ignored" tagged `[thread_pool]`: record `ThreadPool::global().thread_count()`, call `set_default_threads(7)`, REQUIRE the global count is unchanged.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
