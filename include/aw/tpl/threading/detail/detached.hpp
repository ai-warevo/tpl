#pragma once

#include <coroutine>
#include <exception>

namespace aw::tpl::threading::detail {

/**
 * @struct Detached
 * @brief Fire-and-forget coroutine type used for internal helper coroutines (timers, watchers).
 *
 * The coroutine starts SUSPENDED and only runs once @c start() is called, i.e. after the ramp
 * function (the code the compiler generates around the coroutine call) has fully returned.
 * Running the body from inside the ramp would let another thread resume and destroy the frame
 * while the creating thread is still touching it (GCC 15 updates a non-atomic counter in the
 * frame after the initial suspend point). The frame destroys itself on completion.
 */
struct Detached {
    struct promise_type {
        Detached get_return_object() noexcept {
            return Detached{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        // Failing fast is preferable to a silent hang of whoever waits on the result.
        void unhandled_exception() noexcept { std::terminate(); } // LCOV_EXCL_LINE
    };

    /// Runs the coroutine until its first suspension. Must be called exactly once.
    void start() const { handle.resume(); }

    std::coroutine_handle<> handle;
};

} // namespace aw::tpl::threading::detail
