# Changelog

All notable changes to `aw::tpl` are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!--
Maintainer notes (see AGENTS.md, "Changelog & releases"):
- Add every user-visible change under `## [Unreleased]` in the same commit that makes it.
- Categories: Breaking, Added, Changed, Deprecated, Removed, Fixed, Security. Omit empty ones.
- The CI release workflow derives the next version from those categories.
-->

## [Unreleased]

### Added

- Comprehensive test coverage for edge cases: Task stateless exceptions, CancellationToken operations, exception messages, and move-only result handling
- Tests for TaskGroup, ThreadPool, when_any/when_all with heterogeneous types, and co_spawn edge cases

## [4.2.5] - 2026-11-22

### Changed

- use reusable-coverage-badge workflow
- Merge branch 'main' of github.com:ai-warevo/tpl
- Merge pull request #11 from ai-warevo/loktionov129-patch-7
- Update README.md

### Fixed

- enable coverage badge updates for scheduled/manual runs

## [4.2.4] - 2026-11-22

### Changed

- Merge branch 'main' of github.com:ai-warevo/tpl

### Fixed

- resolve build and lint errors in tpl examples

## [4.2.3] - 2026-11-22

### Changed

- use shared template_cpp CMake helpers for project setup and testing
- clang-tidy
- clang-tidy
- Merge branch 'main' of github.com:ai-warevo/tpl
- cmake
- finalize canonical clang-tidy configuration with all exclusions
- merge project-specific clang-tidy exclusions into canonical config
- use tobus .clang-tidy as canonical configuration
- synchronize clang-tidy config from template-cpp

### Fixed

- configure shared warning flags after project

## [4.2.2] - 2026-11-21

### Changed

- lint
- ci
- Merge branch 'main' of github.com:ai-warevo/tpl
- fetch template-cpp from GitHub, support local FETCHCONTENT_SOURCE_DIR

## [4.2.1] - 2026-11-21

### Changed

- fix cmake
- remove local cmake/ duplication, fetch from template_cpp
- Merge pull request #7 from ai-vibeqodez/loktionov129-patch-3
- Update README.md
- Merge branch 'main' of github.com:ai-warevo/tpl
- fix
- Merge pull request #5 from ai-warevo/loktionov129-patch-2
- Update README.md
- Create README.md
- Update README.md
- Merge pull request #4 from ai-vibeqodez/fix/ci-badge-and-sanitizer-comment
- Update pr-test.yml
- ci
- fix
- update badges to reflect new CI wrapper workflows

### Fixed

- refac c++23
- conditional std::move_only_function for clang-18/libc++ compatibility
- escape + in CMake regex for -stdlib=libc++ match
- remove push trigger from pr-test.yml to avoid concurrency deadlock
- CI badge and comment — point to correct workflow, note sanitizer fix
- cmake
- Merge branch 'main' of github.com:ai-warevo/tpl

## [4.2.0] - 2026-11-21

### Added

- add exception reason metadata to exception types
- C++23

### Changed

- remove local release/changelog scripts, now handled by CI
- remove redundant updater scripts
- fix
- replace redundant workflows with catalog reusable workflow wrappers
- integrate reusable CI workflows and add Makefile contract targets
- optimize when_any watcher allocation with direct callbacks
- document Phase 1 and Phase 2 modernization changes
- collect all cancellation callback failures
- make task completion flag atomic for lock-free await_ready()
- use std::ranges idioms for cleaner code
- simplify move-only result tracking with lock-based flag
- use std::move_only_function for cancellation callbacks

## [4.1.0] - 2026-11-18

### Added

- add script updater with auto-sync and pre-flight checks

## [4.0.1] - 2026-11-18

### Fixed

- fix duplicate heading and empty tag list handling

## [4.0.0] - 2026-11-18

### Breaking

- derive custom exceptions from std::exception to fix ASan mismatch

### Fixed

- scripts
- scripts
- use inline CMake commands for Windows/MSVC jobs

### Changed

- add MSVC support to CI/CD workflows
- run all jobs inside ghcr.io/ai-warevo/ci-cpp23 container
- align tpl workflows with tobus pattern, fix ASAN_OPTIONS
- add setup-toolchain (clang18-libcxx, gcc15, msvc) to CI matrix

## [3.1.1] - 2026-11-18

### Changed

- synchronize project configs and add shared scripts

## [3.1.0] - 2026-11-17

### Added

- libc++ support: `-fexperimental-library` is added as a PUBLIC compile/link option when the toolchain uses libc++ (needed for `std::jthread`/`std::stop_token` up to libc++ 18).
- `co_spawn(Task<T>)`, `co_spawn(Task<T>, ErrorHandler)` and `co_spawn(TaskGroup&, Task<T>, ErrorHandler)`: explicit fire-and-forget with optional error reporting and drain tracking.
- `TaskGroup` (`add()`/`done()`/`wait()`/`wait_async()`/`pending()`): counter of in-flight operations that can be drained.
- Example `08_co_spawn`.
- `CONTRIBUTING.md` with development guidelines, testing requirements, and PR process
- CI workflows now use composite actions from the catalog repository (`cmake-build`, `ctest-run`, `gcovr-report`, `coverage-parser`, `badge`, `badge-deploy`, `coverage-html-deploy`, `changelog-release`, `clang-tidy`, `clang-format`)
- `clang-tidy` and `clang-format` composite actions in CI catalog for C++ linting
- Concurrency control added to CI workflows (`cancel-in-progress` for PRs and branch pushes)
- `cmake-build` action now supports `install` and `config` inputs for full CMake lifecycle

### Changed

- Workflow steps refactored to use reusable composite actions instead of inline scripts
- Release job now uses `changelog-release` composite action for CHANGELOG parsing
- `test.yml`: replaced inline configure+build with `cmake-build` composite action
- `ci.yml`: unified Windows and Unix build steps into single `cmake-build` call
- Removed unused `packages: write` permission from `test.yml`

## [3.0.0] - 2026-11-14

### Breaking

- `when_any(Task<A>, Task<B>, ...)` now returns `Task<Task<void>>` (same-type overloads return `Task<Task<T>>`) like
  .NET `Task.WhenAny`, instead of `Task<size_t>` with the winner's index. Compare the winner with `operator==`.
- Headers and namespaces were reorganised to mirror .NET. `<aw/tpl/task.hpp>`, `<aw/tpl/thread_pool.hpp>`,
  `<aw/tpl/when_all.hpp>`, `<aw/tpl/when_any.hpp>` and `<aw/tpl/cancellation.hpp>` are gone. Use
  `<aw/tpl/tpl.hpp>` or the headers under `aw/tpl/threading/` and `aw/tpl/threading/tasks/`. `ThreadPool` and the
  cancellation types now live in `aw::tpl::threading`, `Task`, `when_all`, `when_any` and `async_sleep` in
  `aw::tpl::threading::tasks`.

### Added

- `TimeoutException` and `CancellationTokenSource(std::chrono::milliseconds)`, which cancels itself after the delay.
- `Task<T>` converts implicitly to `Task<void>`, and `operator==` compares task identity across types.
- `CancellationToken::unregister_callback(id)`; `register_callback` now returns the callback id.
- `TaskAlreadyMovedException`, thrown when a move-only result is taken more than once.
- Examples `01_tasks_basics` to `07_timeout` in `examples/`, replacing the single demo.
- Build options `AW_TPL_SANITIZE=address|thread` and `AW_TPL_WARNINGS_AS_ERRORS`, plus a configure-time check of the
  C++23 toolchain.
- Sanitizer (ASan/UBSan, TSan) jobs in CI; `.clang-format`, `.clang-tidy`, `.editorconfig` and `AGENTS.md`.
- README sections on `when_any`, timeouts, project layout and the threading and lifetime contract.

### Changed

- Several awaiters can wait on one task; results are stored in optional storage and returned by value.
- `ThreadPool::set_default_threads` uses an atomic default thread count; tasks keep affinity to their pool.
- Exceptions and cancellation callbacks are isolated: all callbacks run even if one throws and the first exception is
  rethrown by `cancel()`.
- CI runs every `examples/aw_tpl_example_*` binary instead of the old demo.

### Removed

- `examples/main.cpp` (`aw_tpl_demo`), superseded by the numbered examples.

### Fixed

- `ThreadPool` shutdown: workers are joined before the state they use is destroyed, pending timers are flushed and
  work scheduled after shutdown runs inline.
- `when_any` winner-index race and `when_all` returning before every child had finished.
- Lifetime of the sleep awaiter, the promise final awaiter and the task-state finish API.
- Empty/moved-from `Task` guard, move-only results in containers, and blocking a pool worker now throws
  `std::logic_error` instead of deadlocking.
- Constructor safety, timer delay clamping and `noexcept` on awaiters.
- Windows test discovery and the Linux demo path in the Makefile.

## [1.0.1] - 2026-11-13

### Added

- Release notes categories for GitHub releases (`.github/release.yml`).
- More tests for `Task`, `ThreadPool`, `when_any` and cancellation.

### Changed

- README badge order.

## [1.0.0] - 2026-11-13

### Added

- Initial release: eager `Task<T>` / `Task<void>` with `Task<>::run`, `.result()` and `.wait()`.
- `ThreadPool` with a global instance, `set_default_threads` and a non-blocking `async_sleep` timer thread.
- `when_all` and `when_any` over variadic and `std::vector` inputs.
- `CancellationTokenSource`, `CancellationToken`, `cancel_after` and `OperationCanceledException`.
- CMake package (`find_package(aw_tpl)` / `FetchContent`) and CI on Windows, Linux and macOS with GitHub Releases.

[Unreleased]: https://github.com/ai-warevo/tpl/compare/v4.2.5...HEAD
[4.2.5]: https://github.com/ai-warevo/tpl/compare/v4.2.4...v4.2.5
[4.2.4]: https://github.com/ai-warevo/tpl/compare/v4.2.3...v4.2.4
[4.2.3]: https://github.com/ai-warevo/tpl/compare/v4.2.2...v4.2.3
[4.2.2]: https://github.com/ai-warevo/tpl/compare/v4.2.1...v4.2.2
[4.2.1]: https://github.com/ai-warevo/tpl/compare/v4.2.0...v4.2.1
[4.2.0]: https://github.com/ai-warevo/tpl/compare/v4.1.0...v4.2.0
[4.1.0]: https://github.com/ai-warevo/tpl/compare/v4.0.1...v4.1.0
[4.0.1]: https://github.com/ai-warevo/tpl/compare/v4.0.0...v4.0.1
[4.0.0]: https://github.com/ai-warevo/tpl/compare/v3.1.1...v4.0.0
[3.1.1]: https://github.com/ai-warevo/tpl/compare/v3.1.0...v3.1.1
[3.1.0]: https://github.com/ai-warevo/tpl/compare/v3.0.0...v3.1.0
[3.0.0]: https://github.com/ai-warevo/tpl/compare/v2.0.0...v3.0.0
[1.0.1]: https://github.com/ai-warevo/tpl/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/ai-warevo/tpl/releases/tag/v1.0.0
