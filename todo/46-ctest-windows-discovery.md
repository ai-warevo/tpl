# 46 — Windows registers the whole test binary as one ctest entry

**Severity:** Low  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `tests/CMakeLists.txt`

## Exact Location
- `tests/CMakeLists.txt` lines 36-42.

## Problem Description
1. On WIN32 a single `add_test` is used (line 38) while other platforms use `catch_discover_tests`; failures on MSVC are not isolated and `--timeout` per test (needed for deadlock detection) is coarse. Also FetchContent of FakeIt/Catch2 has no `FIND_PACKAGE_ARGS`/offline option.

## Step-by-Step Instructions
1. Replace the platform split by `catch_discover_tests(aw_tpl_tests DISCOVERY_MODE PRE_TEST)` for all platforms (works around the PATH/DLL issue the WIN32 branch avoided) and set `PROPERTIES TIMEOUT 120` for discovered tests.
2. If you cannot verify on Windows, keep the WIN32 branch and only add the TIMEOUT property to the single test.

## Unit Test & Acceptance Criteria
- Linux: `ctest -N` lists individual test cases; all pass. Document in the PR that Windows was/wasn't verified.
- **Done when:** Configuration and build succeed on GCC/Clang; the full existing test suite passes; only the Target Files were modified (`git status`).
