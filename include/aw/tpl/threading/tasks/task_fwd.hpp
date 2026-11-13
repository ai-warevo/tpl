#pragma once

namespace aw::tpl::threading::tasks {

/**
 * @brief Forward declaration of @c Task (the only place carrying the default template argument,
 *        so that @c Task<> is the non-generic task like C# @c System.Threading.Tasks.Task).
 */
template <typename T = void>
class Task;

} // namespace aw::tpl::threading::tasks
