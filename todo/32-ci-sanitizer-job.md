# 32 — CI: add sanitizer jobs

**Severity:** Medium  
**Depends on:** 31 (`cmake-sanitizer-option`)

## Target Files (ONLY these may be edited)
- `.github/workflows/test.yml`

## Exact Location
- .github/workflows/test.yml (add a new job after `quality-and-tests`, lines 14-109).

## Problem Description
1. CI runs only a Debug+coverage build on Linux/GCC (test.yml) and Release without tests on four platforms (ci.yml lines 33-36: `AW_TPL_BUILD_TESTS=OFF`). No sanitizer, no Clang/MSVC/macOS test run exists, so none of the races in this audit can be caught.

## Step-by-Step Instructions
1. Add a job `sanitizers` with a matrix of `address` and `thread` that configures with `-DAW_TPL_SANITIZE=<value> -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAW_TPL_BUILD_EXAMPLES=OFF`, builds, and runs `ctest --output-on-failure --timeout 120`.
2. For the `thread` job run `sudo sysctl vm.mmap_rnd_bits=28` before running tests (known TSan/ASLR incompatibility on newer runners).
3. Do not modify the existing job.

## Unit Test & Acceptance Criteria
- Workflow YAML validates (`python -c "import yaml; yaml.safe_load(open(...))"`). Job succeeds on a branch after the High tasks are merged.
- **Done when:** Configuration and build succeed on GCC/Clang; the full existing test suite passes; only the Target Files were modified (`git status`).
