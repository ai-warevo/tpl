# 42 — CMake: opt-in strict warnings (AW_TPL_WARNINGS_AS_ERRORS)

**Severity:** Low  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `CMakeLists.txt`

## Exact Location
- `CMakeLists.txt` after line 20 (`target_compile_features`).

## Problem Description
1. The build has no warning flags at all. Warnings relevant to coroutines (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`, MSVC `/W4`) are never seen; the current code is warning-clean on GCC 14 so enabling them is safe.

## Step-by-Step Instructions
1. Add `option(AW_TPL_WARNINGS_AS_ERRORS ... OFF)`. When ON add to `aw_tpl` PRIVATE: GCC/Clang `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror`; MSVC `/W4 /WX`. (Tests/examples should not inherit them; keep it PRIVATE.)

## Unit Test & Acceptance Criteria
- Configure with the option ON on GCC: `aw_tpl` library builds without warnings.
- **Done when:** Configuration and build succeed on GCC/Clang; the full existing test suite passes; only the Target Files were modified (`git status`).
