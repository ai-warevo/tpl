# Contributing to aw::tpl

Thank you for your interest in contributing to **aw::tpl** — the C++23 library that brings C# Task Parallel Library ergonomics to modern C++ coroutines.

## 🚀 Quick Start

1. **Fork** the repository
2. **Clone** your fork:
   ```bash
   git clone https://github.com/<your-username>/tpl.git
   cd tpl
   ```
3. **Create a feature branch**:
   ```bash
   git checkout -b feature/amazing-feature
   ```
4. **Build and test**:
   ```bash
   cmake -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAW_TPL_WARNINGS_AS_ERRORS=ON
   cmake --build build/dev --parallel
   ctest --test-dir build/dev --output-on-failure --parallel 4
   ```

## 📋 Development Discipline

### Coding Standards

- **C++23 idiomatic style**: concepts/requires over SFINAE, coroutines, `std::jthread`, RAII everywhere
- **Naming conventions**: types `PascalCase`, functions/variables `snake_case`, private members `trailing_underscore_`
- **Formatting**: 4 spaces, 120 cols, attached braces (see `.clang-format`, `.editorconfig`)
- **Headers**: standard include guards in `UPPER_SNAKE_CASE` (`#ifndef AW_TPL_...` / `#define AW_TPL_...` / `#endif  // AW_TPL_...`); include groups (own header → `<aw/tpl/...>` → `<std>`), blank line between groups

### Architecture Rules

- **Namespaces map 1:1 to directories** (mirroring .NET):
  - `aw::tpl` → `include/aw/tpl/`, `src/`
  - `aw::tpl::threading` → `.../threading/`
  - `aw::tpl::threading::tasks` → `.../threading/tasks/`
  - `*::detail` → `*/detail/` (internal, not public API)
- **One public entity per header** (e.g., `thread_pool.hpp`, `cancellation_token.hpp`)
- **Non-template code**: declared in header, defined in mirrored `src/` path
- **Dependencies flow**: `tasks` → `threading` → `aw::tpl` root (never reverse)

### Testing Requirements

- **Every new feature needs tests** in `tests/` (tag with `[area]`)
- **Target >90% line coverage** (currently 100% — do not regress)
- **Tests must pass** under ASan/UBSan/TSan with 120s ctest timeout
- **Test concurrency deterministically** (latches, atomics, `std::binary_semaphore`), not with sleeps

### Documentation

- **Every public entity gets Doxygen** (`@class`, `@brief`, `@tparam`, `@param[in]`, `@return`, `@throws`)
- **State the .NET equivalent** in Doxygen (`equivalent to C# @c Task.X`)
- **Inline comments**: minimal, only for non-obvious logic or the *why* behind a choice
- **Update README** when public behaviour changes

## 🔄 Pull Request Process

1. **Update CHANGELOG.md** — add every user-visible change under `## [Unreleased]` in the same commit
2. **Run local tests** before pushing:
   ```bash
   cmake --build build/dev --parallel
   ctest --test-dir build/dev --output-on-failure --parallel 4
   ```
3. **Ensure warnings-as-errors pass**:
   ```bash
   cmake -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAW_TPL_WARNINGS_AS_ERRORS=ON
   cmake --build build/dev --parallel
   ```
4. **Request review** from maintainers
5. **Minimum one approving review** required
6. **All CI checks must pass** (build matrix, tests, sanitizers, coverage)

## 📝 Conventional Commits

Use [Conventional Commits](https://www.conventionalcommits.org/) format:

```
type(scope): description

- `feat(threading): add async_sleep with cancellation support`
- `fix(tasks): fix when_any race condition on winner selection`
- `docs(readme): update integration example`
- `refactor(detail): simplify task_state_base`
- `test(cancellation): add timeout callback tests`
```

**Types**: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`, `revert`

## 🧪 Adding Tests

1. Create test file in `tests/` (e.g., `test_new_feature.cpp`)
2. Register in `tests/CMakeLists.txt` (already handled by `aw_tpl_tests` target)
3. Tag tests with area: `TEST_CASE("[threading][cancellation]", "[threading]")`
4. Cover: success paths, failure modes, edge cases, cancellation before/during/after

## 🔧 Debugging

- **Sanitizers**: CI runs ASan/UBSan and TSan; enable locally:
  ```bash
  cmake -B build/san -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAW_TPL_SANITIZE=address
  cmake --build build/san --parallel
  ctest --test-dir build/san --output-on-failure
  ```
- **Coverage**: generate HTML report:
  ```bash
  cmake -B build/cov -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAW_TPL_ENABLE_COVERAGE=ON
  cmake --build build/cov --parallel
  ctest --test-dir build/cov --output-on-failure
  gcovr -r build/cov/ --filter "include/aw/tpl/" --filter "src/" --html-details htmlcov/index.html
  ```

## ❓ Questions?

- Open an [Issue](https://github.com/ai-warevo/tpl/issues) with the `question` label
- Check existing issues and [README](README.md) for common patterns

## 🙏 Thank You!

Your contributions make aw::tpl better for everyone. Every bug report, feature request, and pull request is valued.
