# 43 — CMake: fail early when the toolchain lacks jthread/stop_token/semaphore/coroutines

**Severity:** Low  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `CMakeLists.txt`

## Exact Location
- `CMakeLists.txt` lines 12-20 (`find_package(Threads)` and `target_compile_features`).

## Problem Description
1. The CI matrix includes AppleClang whose libc++ may lack `std::jthread`/`std::stop_token` depending on version (`#include <stop_token>` compile error deep in a header); GCC 10 needs `-fcoroutines`. A late, cryptic compile error is the result.

## Step-by-Step Instructions
1. After `find_package(Threads)` use `include(CheckCXXSourceCompiles)` with CMAKE_CXX_STANDARD 20 to compile a small program that uses `std::jthread`, `std::stop_token`, `std::binary_semaphore` and a trivial coroutine; on failure `message(FATAL_ERROR ...)` naming the minimum compilers. For GCC < 11 add `-fcoroutines` PUBLIC.

## Unit Test & Acceptance Criteria
- Configure succeeds on GCC 14; temporarily simulate failure by forcing the check variable to false and see the readable error (then revert).
- **Done when:** Configuration and build succeed on GCC/Clang; the full existing test suite passes; only the Target Files were modified (`git status`).
