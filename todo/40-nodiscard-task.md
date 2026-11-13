# 40 — Mark Task [[nodiscard]]

**Severity:** Low  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `include/aw/tpl/threading/tasks/task.hpp`
- `tests/test_task.cpp`

## Exact Location
- `include/aw/tpl/threading/tasks/task.hpp` `class Task` (line 22), `Task::run` (line 111).

## Problem Description
1. Dropping a returned Task silently loses its exception (fire-and-forget with swallowed errors). A mistaken `when_all(...);` without `co_await` compiles without a warning.

## Step-by-Step Instructions
1. Add `[[nodiscard]]` to the class declaration of `Task` (also on the forward declaration in promise.hpp line 14 is NOT required; attribute on the class definition is enough).
2. Fix any call in the repository tests/examples that discards a Task (grep the tests; compile with `-Wall -Werror=unused-result`); if the repository's own library code discards one deliberately (the `spawn_when_any_watcher` returns a different type), leave it.

## Unit Test & Acceptance Criteria
- Compiles with no new warnings; add to the test file a `static_assert` that `std::is_class_v<Task<int>>` (sanity) – real verification is the warning-clean build.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
