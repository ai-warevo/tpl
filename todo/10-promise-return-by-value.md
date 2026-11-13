# 10 — Promise::return_value: greedy forwarding template breaks `co_return {…}` and move-only/nondefault types

**Severity:** High  
**Depends on:** 09 (`task-state-optional-value`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/promise.hpp`
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/promise.hpp` `Promise<T>::return_value` (lines 41-45).

## Problem Description
1. `template <typename U> void return_value(U&& v)` cannot deduce `U` from a braced-init-list, so `co_return {1, 2};` in a `Task<std::pair<int,int>>` fails to compile (probe: `no matching function for call to return_value(<brace-enclosed initializer list>)`).
2. It assigns into the pre-existing `value` object (needs default constructor + assignment); with task 09 (`task-state-optional-value`) the value is an `std::optional<T>` and should be constructed in place.

**Audit evidence:** Compile probes: non-default-constructible T and `co_return {1,2}` both fail to compile today.

## Step-by-Step Instructions
1. Replace the template by a non-template `void return_value(T v)` (T by value) that does `this->state->value.emplace(std::move(v))`.
2. Add a `static_assert` in `Promise<T>` that `T` is not a reference type, with a clear message (Task<T&> was never supported; the probe showed an unreadable error).
3. Do not call `complete()` anywhere (task 03 (`promise-final-awaiter`) already removed it).
4. Keep `Promise<void>` untouched.

## Unit Test & Acceptance Criteria
- In `tests/test_task.cpp` add three test cases tagged `[task][value]`: (1) a coroutine returning `Task<std::pair<int,int>>` that does `co_return {3, 4};` → REQUIRE result equals pair(3,4); (2) a local `struct NoDefault { explicit NoDefault(int v); int v; };` and `Task<NoDefault>` that returns `NoDefault{5}` → REQUIRE `.result().v == 5`; (3) `Task<std::string>` returning a `const char*` literal → REQUIRE equals "abc".
- Existing move-only test still passes.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
