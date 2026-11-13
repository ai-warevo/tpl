# 31 — Add AW_TPL_SANITIZE option (address / thread) to CMake

**Severity:** Medium  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `CMakeLists.txt`

## Exact Location
- `CMakeLists.txt` after the coverage option (lines 40-45).

## Problem Description
1. There is no way to build the library and tests with sanitizers; all the lifetime and race bugs in this audit are only detectable that way. The coverage option applies `--coverage -O0 -g` PUBLIC to every consumer (line 43), a similar PUBLIC option is needed for sanitizers so tests are instrumented too.

## Step-by-Step Instructions
1. Add a cache STRING option `AW_TPL_SANITIZE` with allowed values `none` (default), `address` (ASan + UBSan) and `thread` (TSan); set the STRINGS property.
2. When not `none` and the compiler is GNU or Clang: add `-fsanitize=<kinds> -fno-omit-frame-pointer -g` as PUBLIC compile options and `-fsanitize=<kinds>` as PUBLIC link options of `aw_tpl`. Error out with `message(FATAL_ERROR ...)` for other compilers and when combined with `AW_TPL_ENABLE_COVERAGE`.
3. Do not change any existing option.

## Unit Test & Acceptance Criteria
- Configure three build dirs (`none`, `address`, `thread`), build and run `ctest`; the `address` build passes; the `thread` build passes once all High tasks are done (it cannot run inside some containers because of ASLR; run on CI or a normal host).
- **Done when:** Configuration and build succeed on GCC/Clang; the full existing test suite passes; only the Target Files were modified (`git status`).
