# 18 — ThreadPool constructor leaks running threads if a later thread fails to start; move thread members last

**Severity:** Medium  
**Depends on:** 06 (`thread-pool-schedule-after-stop`)

## Target Files (ONLY these may be edited)
- `include/aw/tpl/thread_pool.hpp`
- `src/thread_pool.cpp`

## Exact Location
- `src/thread_pool.cpp` `ThreadPool::ThreadPool` (lines 25-34): `workers_.emplace_back` loop lines 29-31, `timer_thread_` assignment line 33. `include/aw/tpl/thread_pool.hpp` members lines 123-131.

## Problem Description
1. If `emplace_back` (or the timer thread creation) throws `std::system_error`/`std::bad_alloc` after some workers already started, the constructor exits with an exception, so the destructor never runs. Members are destroyed in reverse declaration order: `queue_cv_` and `queue_mtx_` (declared after `workers_`) are destroyed BEFORE `workers_`, and each already-started worker is blocked in `queue_cv_.wait`, which a `std::jthread` destructor (stop request + join, no notify) cannot wake: undefined behaviour or a permanent hang.
2. The worker lambdas capture `this` of a partially constructed object.

## Step-by-Step Instructions
1. In the header, move the two thread members (`workers_` and `timer_thread_`) to the END of the member list so that they are destroyed first, after all mutexes/condvars/queues remain valid (keep a comment explaining why).
2. In the constructor wrap the worker/timer creation in a try/catch(...) block: in the handler lock the respective mutexes, request stop on all already started threads, notify both condition variables, join every joinable thread, then `throw;`.
3. Keep behaviour for `thread_count == 0` (fallback 2).
4. Update the class doc comment in the header (shutdown order: timer thread first, then workers).

## Unit Test & Acceptance Criteria
- No deterministic way to make `std::jthread` creation fail, so there is no new test file. Acceptance: the whole existing suite passes, including the create/destroy stress test added by task 04 (`thread-pool-dtor-join-order`), under ASan (and TSan where available).
- Review checklist: confirm by reading that no code path can leave a started thread unjoined after an exception.
- **Done when:** All existing tests plus the new tests pass (`make test`, Debug). The same test binary also passes when the project is configured with AddressSanitizer + UndefinedBehaviorSanitizer (e.g. extra CMake flags `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"`) with zero reports. No file outside the Target Files list was modified (`git status` shows only those files plus nothing else).
