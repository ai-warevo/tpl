# CLAUDE.md

Guidance for working on `aw::tpl`. Read `README.md` for the user-facing API; this file is about *how to change the repo*.

## Identity & philosophy

`aw::tpl` is a zero-dependency C++20 library that brings the **C# / .NET Task Parallel Library** mental model to C++ coroutines: eager `Task<T>`/`Task<void>`, `ThreadPool`, `Task<>::run` (= `Task.Run`), `async_sleep` (= `Task.Delay`), `when_all`/`when_any` (= `Task.WhenAll`/`WhenAny`, the latter returning `Task<Task<T>>`), `CancellationToken(Source)`, `OperationCanceledException`/`TimeoutException`.

When designing a feature:
- Start from the .NET equivalent. Mirror its name (snake_cased), semantics, return types and exception behaviour; deviate only where C++ forces it, and say so in the Doxygen (`equivalent to C# @c Task.X`).
- Tasks are **eager** and run on the pool; awaiting never blocks a worker.
- Keep the public surface small and boring: no new dependencies, no macros in public headers.
- A new public feature also needs a README mention and, if it is a distinct concept, an `examples/NN_*.cpp`.

## Build & test (fast local loop)

```sh
cmake -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAW_TPL_WARNINGS_AS_ERRORS=ON
cmake --build build/dev --parallel
ctest --test-dir build/dev --output-on-failure --parallel 4      # whole suite ~1s
ctest --test-dir build/dev -R "when_any" --output-on-failure      # subset (Catch2 tests are discovered individually)
```

- `make test` / `make demo` (Makefile) do a `--fresh` configure into `build/linux/{test,demo}`; examples: `cmake -B build/ex -DAW_TPL_BUILD_EXAMPLES=ON` then run `build/ex/examples/aw_tpl_example_*`.
- Catch2 is fetched via `FetchContent` (network needed on the first configure).
- Always build with `AW_TPL_WARNINGS_AS_ERRORS=ON` (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror`) before finishing.
- **Leave to CI:** sanitizers (`AW_TPL_SANITIZE=address|thread`), coverage (`AW_TPL_ENABLE_COVERAGE`, gcovr), MSVC/macOS builds, install/packaging. Don't run them locally unless debugging a race/UB report.
- Adding a source or header? Register it in the root `CMakeLists.txt` (`add_library` sources **and** the `FILE_SET HEADERS` list); new tests go in `tests/CMakeLists.txt`, new examples in `examples/CMakeLists.txt`.

## Testing discipline

- Every new feature, edge case, error path and branch gets a Catch2 test in `tests/` (tag with `[area]`). No untested code.
- CI requires **>90% line coverage; it is currently 100%** — do not regress it. Defensive code that can't be reached is a smell: remove it or make it testable.
- Test concurrency deterministically (latches, atomics, `std::binary_semaphore`), not with sleeps as synchronization. Tests must also pass under ASan/UBSan/TSan, and tests have a 120 s ctest timeout, so a deadlock fails rather than hangs.
- Cover failure modes explicitly: exceptions propagating through `co_await`, cancellation before/during/after, moved-from tasks, move-only results, `TaskAlreadyMovedException`.

## Architecture & layout rules

Namespaces map 1:1 to directories, mirroring .NET:

| C++ | .NET | Path |
| --- | --- | --- |
| `aw::tpl` | `System` | `include/aw/tpl/`, `src/` |
| `aw::tpl::threading` | `System.Threading` | `.../threading/` |
| `aw::tpl::threading::tasks` | `System.Threading.Tasks` | `.../threading/tasks/` |
| `*::detail` | *(internal)* | `.../detail/` next to the owning namespace |

- **Granularity:** roughly one public entity per header (`thread_pool.hpp`, `cancellation_token.hpp`, `when_any.hpp`, …); file names are `snake_case` of the type name. `tpl.hpp` is the umbrella header — add new public headers there.
- **All internal helpers, state, promises, awaiters and scheduling glue live in `detail/`** (`aw::tpl::...::detail`). Nothing in `detail/` is public API; never expose it in a public signature beyond what templates unavoidably need.
- **`.hpp`/`.cpp` separation:** non-template code is declared in the header and defined in the mirrored path under `src/` (`include/aw/tpl/x/y.hpp` → `src/x/y.cpp`). Only templates, concepts and trivial one-line inline accessors stay in headers.
- Headers start with `#pragma once`; a `.cpp` includes its own header first. Include groups: own header, then `<aw/tpl/...>`, then `<std>` headers, blank line between groups (in tests: Catch2 first).
- Keep files small and single-purpose; split rather than grow. Dependencies flow `tasks` → `threading` → `aw::tpl` root (never the reverse), and `detail` never depends on public convenience headers.

## Documentation & comments

- **Every public class, function, concept, constructor and exception type gets a Doxygen `/** ... */` block** using the existing tags: `@class`/`@struct`, `@brief`, `@tparam`, `@param[in]`, `@return`, `@throws`, and `@c` for code. State the .NET equivalent and the thread-safety/lifetime guarantees where relevant. Brief `/** */` on `detail/` types is encouraged too.
- Inline `//` comments are **minimal**: use them only for non-obvious logic, subtle concurrency/lifetime/ordering reasons, or the *why* behind a choice (e.g. "threads are declared last so they are joined first"). Never narrate what the code obviously does (`// increment counter`, `// print to console`). Prefer better names over comments. Use `///` only for one-line member docs as in `thread_pool.hpp`.
- Keep README tables/examples in sync when public behaviour changes.

## C++20 conventions & style

- Idiomatic modern C++20: concepts/`requires` over SFINAE (`requires std::invocable<Func>`, `requires (sizeof...(Ts) > 0)`), `if constexpr`, coroutines, `std::jthread`/`std::stop_token`, `std::binary_semaphore`, RAII everywhere (no raw `new`/`delete`, no manual lock/unlock — use `std::lock_guard`/`std::scoped_lock`).
- `[[nodiscard]]` on `Task` and on pure getters/factories; mark `noexcept` where truly non-throwing; `explicit` on single-arg constructors; delete copy where ownership is unique.
- Naming: types `PascalCase`; functions/variables/files `snake_case`; private data members `trailing_underscore_`; plain `detail` structs use bare member names; namespaces lowercase. Exceptions derive from `std` exceptions and carry the .NET-style default message.
- Formatting (see `.clang-format`, `.editorconfig`): 4 spaces, 120 cols, attached braces, `Type* p` / `Type& r`, `public:` flush with `class`, no indentation inside namespaces, closing comments `} // namespace aw::tpl`, constructor initializer lists break before `:`. Short `if (x) stmt;` and one-line inline accessors are fine.
- The repo mixes LF and CRLF files; **preserve each file's existing line endings** (don't normalise whole files in a feature change).

### Dev tooling (all optional, none run in CI)

- `.editorconfig` – indentation/charset/whitespace basics for any editor.
- `.clang-format` – matches the style above (needs clang-format ≥ 15). Format only files you touch (`clang-format -i path`); don't reformat the tree in unrelated diffs.
- `.clang-tidy` – small, low-noise set (bugprone, performance, coroutine-lifetime checks). Run with `clang-tidy -p build/dev <file>`. Capturing coroutine lambdas stored in a named variable in tests are intentional (see below).
- Top-level CMake sets `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, so `build/dev/compile_commands.json` exists for clangd/clang-tidy (point clangd at it with `--compile-commands-dir=build/dev`, or symlink it to the repo root; the file is git-ignored).

## Threading & lifetime invariants (do not break)

- **Never block on a pool worker.** `.result()`/`.wait()` on an unfinished task from a pool thread throws `std::logic_error`. Inside coroutines use `co_await`; block only on non-pool threads (`main`). New blocking-style APIs must keep this guard.
- **Don't invoke capturing coroutine lambdas as temporaries** (`[&]() -> Task<void> { co_await ...; }()` — CP.51): the closure dies at the `;` while the frame still references it. Pass state as parameters (`[](int& x) -> Task<void> {...}(x)`) or use `Task<>::run([&]{...})`, or keep the lambda alive in a named variable that outlives the task.
- Coroutine parameters are copied into the frame: don't take references/views to temporaries in coroutine signatures unless the referent provably outlives the task.
- Shared task state is guarded by `TaskStateBase::mtx`; continuations are resumed outside the lock. The frame is destroyed only when the last `Task`/awaiter reference drops — never from inside its own ramp.
- `ThreadPool` shutdown is ordered: timer thread stopped → pending timers flushed → workers drain queue → join. Work scheduled during shutdown runs inline. Threads must stay the **last** members so they are joined first.
- Cancellation callbacks run synchronously on the canceling (or timer) thread; all run even if one throws, first exception rethrown by `cancel()`.
- `when_all` awaits every child before rethrowing the first failure (by index); `when_any` never consumes the winner's result and never cancels losers. Move-only results can be taken once → `TaskAlreadyMovedException`.
- Any new shared-state code must be reasoned about for TSan (CI runs it): use mutexes/atomics with explicit ordering rationale in a short comment.
