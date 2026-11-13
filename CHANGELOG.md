# Changelog

All notable changes to `aw::tpl` are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!--
Maintainer notes (see CLAUDE.md, "Changelog & releases"):
- Add every user-visible change under `## [Unreleased]` in the same commit that makes it.
- Categories: Breaking, Added, Changed, Deprecated, Removed, Fixed, Security. Omit empty ones.
- scripts/release.sh / scripts/release.ps1 derive the next version from those categories.
-->

## [Unreleased]

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
  C++20 toolchain.
- Sanitizer (ASan/UBSan, TSan) jobs in CI; `.clang-format`, `.clang-tidy`, `.editorconfig` and `CLAUDE.md`.
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

[Unreleased]: https://github.com/ai-warevo/tpl/compare/v1.0.1...HEAD
[1.0.1]: https://github.com/ai-warevo/tpl/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/ai-warevo/tpl/releases/tag/v1.0.0
