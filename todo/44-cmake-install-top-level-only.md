# 44 — CMake: install rules and package config run when used via FetchContent

**Severity:** Low  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `CMakeLists.txt`

## Exact Location
- `CMakeLists.txt` lines 47-82.

## Problem Description
1. `install(...)`, `write_basic_package_version_file` and `file(WRITE ...aw_tplConfig.cmake)` run even when the project is consumed as a subproject, polluting the parent's install tree and exporting `aw_tpl` into parent export sets.

## Step-by-Step Instructions
1. Wrap the whole `Install & Package Export Rules` section in `if(AW_TPL_IS_TOP_LEVEL)`; also add an option `AW_TPL_INSTALL` (default `${AW_TPL_IS_TOP_LEVEL}`) and use that in the condition.

## Unit Test & Acceptance Criteria
- Configure as top-level: `cmake --install` still works. Configure through a tiny parent project using `add_subdirectory` → no install rules are registered (`cmake --install` of the parent installs nothing from aw_tpl).
- **Done when:** Configuration and build succeed on GCC/Clang; the full existing test suite passes; only the Target Files were modified (`git status`).
