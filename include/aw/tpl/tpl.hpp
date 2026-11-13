#pragma once

/**
 * @file tpl.hpp
 * @brief Umbrella header including all C# TPL-style C++20 coroutine primitives from @c aw::tpl.
 *
 * Layout (mirrors the .NET namespaces):
 *  - @c aw::tpl                    ~ @c System                  (exceptions)
 *  - @c aw::tpl::threading         ~ @c System.Threading        (ThreadPool, CancellationToken[Source])
 *  - @c aw::tpl::threading::tasks  ~ @c System.Threading.Tasks  (Task, Task<T>, when_all, when_any, async_sleep)
 *  - @c *::detail                  internal implementation details, not part of the public API
 */

#include <aw/tpl/operation_canceled_exception.hpp>
#include <aw/tpl/timeout_exception.hpp>

#include <aw/tpl/threading/thread_pool.hpp>
#include <aw/tpl/threading/cancellation_token.hpp>
#include <aw/tpl/threading/cancellation_token_source.hpp>

#include <aw/tpl/threading/tasks/task.hpp>
#include <aw/tpl/threading/tasks/task_already_moved_exception.hpp>
#include <aw/tpl/threading/tasks/async_sleep.hpp>
#include <aw/tpl/threading/tasks/when_all.hpp>
#include <aw/tpl/threading/tasks/when_any.hpp>
