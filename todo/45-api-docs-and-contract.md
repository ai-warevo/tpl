# 45 — Document the concurrency contract (blocking waits, shutdown, callbacks, single-use move-only)

**Severity:** Low  
**Depends on:** 13 (`task-value-taken-extract`), 17 (`when-all-variadic-wait-siblings`), 05 (`thread-pool-flush-timers-on-shutdown`), 40 (`nodiscard-task`)

## Target Files (ONLY these may be edited)
- `README.md`

## Exact Location
- `README.md` section `C++20 Coroutine Lifetime Note` (lines 81-85) and features (16-23).

## Problem Description
1. README line 20 advertises `.result()`/`.wait()` without any restriction while blocking on a worker deadlocks / throws; there is no statement about shutdown behaviour, cancellation callbacks (run on the cancelling thread, may throw), multiple awaiters, move-only single-use results, `when_all` waiting for all children, and `when_any` leaving losers running.

## Step-by-Step Instructions
1. Add a section `Threading & lifetime contract` as a bullet list covering exactly: (a) never call `.result()`/`.wait()` from a pool worker; (b) pool destruction resumes pending timers immediately; (c) cancel callbacks run synchronously on the cancelling thread and are all run even if one throws; (d) multiple awaiters allowed; move-only result can be taken once; (e) `when_all` waits for every child and rethrows the first failure; (f) `when_any` does not cancel the losers; (g) `Task` is `[[nodiscard]]`.
2. Only describe behaviour that exists after the High/Medium tasks are merged.

## Unit Test & Acceptance Criteria
- Markdown renders; every bullet corresponds to a test name in the suite (list them in the PR description).
- **Done when:** Configuration and build succeed on GCC/Clang; the full existing test suite passes; only the Target Files were modified (`git status`).
