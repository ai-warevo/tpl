# 39 — Missing includes / not self-contained headers

**Severity:** Low  
**Depends on:** None

## Target Files (ONLY these may be edited)
- `include/aw/tpl/when_any.hpp`
- `include/aw/tpl/thread_pool.hpp`

## Exact Location
- `include/aw/tpl/when_any.hpp`: uses `std::tuple`, `std::get`, `std::index_sequence`, `std::make_tuple` (lines 119-124), `schedule_on_pool` (line 55) and `size_t`, but includes none of `<tuple>`, `<cstddef>`, `<aw/tpl/thread_pool.hpp>`.
- `include/aw/tpl/thread_pool.hpp`: uses `std::greater<>` (line 129) without `<functional>` and `size_t` without `<cstddef>`.

## Problem Description
1. The headers compile today only through transitive includes (libstdc++ specifics). MSVC/libc++ may fail.

## Step-by-Step Instructions
1. Add `<tuple>`, `<utility>`, `<cstddef>` and `<aw/tpl/thread_pool.hpp>` to when_any.hpp; add `<functional>` and `<cstddef>` to thread_pool.hpp (and `<atomic>` if the atomic default thread count task was done).
2. Do not reorder or remove existing includes.

## Unit Test & Acceptance Criteria
- Compile each header standalone: `echo '#include <aw/tpl/when_any.hpp>' | g++ -std=c++20 -fsyntax-only -Iinclude -x c++ -` and same for thread_pool.hpp, when_all.hpp, cancellation.hpp, task.hpp: all succeed. Full suite passes.
- **Done when:** Configuration and build succeed on GCC/Clang; the full existing test suite passes; only the Target Files were modified (`git status`).
