# 11 — await_resume copies vector<unique_ptr<…>> (is_copy_constructible_v lies) → when_all of move-only tasks does not compile

**Severity:** High  
**Depends on:** 10 (`promise-return-by-value`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/task.hpp`
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/task.hpp` `Task::await_resume` lines 71-77 (`std::is_copy_constructible_v<T>` on line 72).

## Problem Description
1. `std::is_copy_constructible_v<std::vector<std::unique_ptr<int>>>` is true (the vector's copy constructor is declared) although instantiating it fails. So `co_await`/`result()` of a `Task<std::vector<std::unique_ptr<int>>>` takes the copy branch and the build breaks inside libstdc++ (`static assertion failed: result type must be constructible from input type`).
2. Consequence: `when_all(std::vector<Task<std::unique_ptr<int>>>)` (when_all.hpp:37-55, returns Task<std::vector<T>>) cannot be used with move-only element types. Probe confirmed the compile error.

**Audit evidence:** Compile probe: `when_all(std::move(vector<Task<unique_ptr<int>>>)).result()` -> `static assertion failed: result type must be constructible from input type`.

## Step-by-Step Instructions
1. Create in `task.hpp` (inside `namespace task_detail`, NOT `detail`, to avoid clashing with `aw::tpl::detail`) a constexpr trait `is_truly_copyable_v<T>`: defaults to `std::is_copy_constructible_v<T>`; for `std::vector<E>`, `std::optional<E>`, `std::array<E,N>`, `std::pair<A,B>`, `std::tuple<Es...>`, `std::variant<Es...>` and `std::deque`/`std::list` it recurses into the element types (all must be truly copyable).
2. Use the new trait instead of `std::is_copy_constructible_v<T>` in `await_resume`.
3. Add the includes needed (`<vector>`, `<optional>`, `<array>`, `<utility>`, `<tuple>`, `<variant>`, `<deque>`, `<list>`).

## Unit Test & Acceptance Criteria
- In `tests/test_task.cpp` add TEST_CASE "Task of vector<unique_ptr> is movable out" tagged `[task][move]`: coroutine returns a vector with two unique_ptr<int> (values 1, 2) via `co_return` of a local; `.result()` and REQUIRE size 2 and values.
- Add `#include <aw/tpl/when_all.hpp>` and TEST_CASE "when_all over move-only tasks" tagged `[when_all][move]`: vector of three `Task<std::unique_ptr<int>>`; REQUIRE results vector holds 3 non-null pointers with the right values.
- Add a `static_assert` in the test file that the trait reports false for `std::vector<std::unique_ptr<int>>` and true for `std::vector<int>` (trait is in `task_detail`).
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
