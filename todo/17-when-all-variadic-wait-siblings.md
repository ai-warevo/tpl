# 17 — when_all(variadic): argument evaluation order, early rethrow and lifetime of per-task helper tasks

**Severity:** High  
**Depends on:** 16 (`when-all-vector-wait-siblings`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/when_all.hpp`
- `tests/test_when_all.cpp`

## Exact Location
- `include/aw/tpl/when_all.hpp` variadic overload (lines 65-87): `await_one` lambda lines 74-81, outer lambda line 83-85 `co_return std::make_tuple((co_await helper(list))...)`.

## Problem Description
1. Order of evaluation of the function arguments of `std::make_tuple` is unspecified, so the `co_await`s may happen in any order; with several `co_await`s inside one call expression some compiler versions have had code-generation bugs for coroutines.
2. If one `co_await helper(list)` throws, the remaining helper tasks (already started eagerly) are not awaited; they still reference `list` elements living in the outer coroutine frame that is destroyed on the exception path → use-after-free; their exceptions are lost.

## Step-by-Step Instructions
1. Rewrite the non-void branch so it first starts all helper tasks, storing the resulting `Task`s in a `std::tuple`/`std::array`-like local in index order (use an index_sequence expansion with a comma fold; one helper per task), then awaits them one by one in index order, each inside try/catch remembering the first exception, and builds the result tuple only if no exception occurred (rethrow the first one after all are awaited).
2. Helpers must take the task by VALUE or hold it in the outer frame that is kept alive until every helper completed (i.e. never destroy the outer frame before all helpers were awaited).
3. Keep the return type `Task<std::tuple<detail::WhenAllElementT<Ts>...>>` and the all-void shortcut unchanged.

## Unit Test & Acceptance Criteria
- In `tests/test_when_all.cpp` add TEST_CASE "variadic when_all waits for all children on failure" tagged `[when_all][exceptions]`: arguments (Task<int> throwing immediately, Task<int> sleeping 60 ms then incrementing an atomic counter passed by reference); REQUIRE_THROWS_AS then REQUIRE counter equals 1. Run under ASan.
- Add a test where the *last* argument fails to confirm earlier results are not lost silently (still throws).
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
