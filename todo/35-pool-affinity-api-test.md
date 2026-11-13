# 35 — Test schedule_on_pool(pool, handle) targets the requested pool

**Severity:** Low  
**Depends on:** 34 (`pool-affinity-api`)

## Target Files (ONLY these may be edited)
- `tests/test_thread_pool.cpp`

## Exact Location
- `tests/test_thread_pool.cpp` (append at end).

## Problem Description
1. The overload added by task 34 (`pool-affinity-api`) has no test.

## Step-by-Step Instructions
1. Append a TEST_CASE tagged `[thread_pool][affinity]`. Define, inside the test, a small awaiter struct whose `await_suspend` calls the new `detail::schedule_on_pool(pool_pointer, handle)` with a pool pointer stored in the awaiter and whose other two awaiter functions do nothing.
2. Create a local `ThreadPool pool(1)`. Start a `Task<ThreadPool*>` coroutine (on the global pool, from the main thread) that co_awaits that awaiter and then returns `detail::current_pool`. REQUIRE the result equals `&pool`.
3. Add a second section where the pool pointer is null: REQUIRE the result is a non-null pool that is not `&pool` (the global pool).

## Unit Test & Acceptance Criteria
- Both sections pass; `detail::current_pool` is null on the main thread afterwards.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
