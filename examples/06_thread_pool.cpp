// ThreadPool: the global pool, default thread count, and a private pool of your own.
#include <aw/tpl/tpl.hpp>

#include <iostream>
#include <thread>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace aw::tpl::threading::tasks;

Task<int> work(int v) {
    co_await async_sleep(20ms);
    co_return v + 1;
}

int main() {
    // Must be called before the global pool is first used; afterwards it is ignored.
    ThreadPool::set_default_threads(4);
    std::cout << "global pool threads: " << ThreadPool::global().thread_count() << "\n";

    // A coroutine started from a non-pool thread runs on the global pool.
    std::cout << "work(1) = " << work(1).result() << "\n";

    // Own pool: coroutines started *from a worker of this pool* (and their continuations,
    // timers and cancellations) stay on it. Task<>::run is the easy way to start such a coroutine.
    {
        ThreadPool pool(2);
        std::cout << "private pool threads: " << pool.thread_count() << "\n";
    }   // destruction resumes pending timers, drains queued work and joins every thread

    // Blocking on a pool worker is a bug and throws std::logic_error instead of deadlocking.
    auto bad = []() -> Task<void> {
        try {
            (void)work(1).result();
        } catch (const std::logic_error& ex) {
            std::cout << "blocking on a worker: " << ex.what() << "\n";
        }
        co_return;
    };
    bad().wait();
    return 0;
}
