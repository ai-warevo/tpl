#include <aw/tpl/threading/thread_pool.hpp>

#include <aw/tpl/threading/detail/pool_scheduling.hpp>

#include <atomic>
#include <thread>

namespace aw::tpl::threading {

namespace {

std::atomic<std::size_t>& default_thread_count() noexcept {
    static std::atomic<std::size_t> count{[] {
        const std::size_t hw = std::thread::hardware_concurrency();
        return hw > 0 ? hw : std::size_t{2};
    }()};
    return count;
}

thread_local ThreadPool* t_current_pool = nullptr;

} // namespace

namespace detail {

ThreadPool* current_pool() noexcept {
    return t_current_pool;
}

CurrentPoolScope::CurrentPoolScope(ThreadPool* pool) noexcept
    : previous_(t_current_pool) {
    t_current_pool = pool;
}

CurrentPoolScope::~CurrentPoolScope() noexcept {
    t_current_pool = previous_;
}

void schedule_on_pool(ThreadPool* pool, std::coroutine_handle<> h) {
    if (!pool) {
        pool = current_pool() ? current_pool() : &ThreadPool::global();
    }
    pool->schedule(h);
}

void schedule_on_pool(std::coroutine_handle<> h) {
    schedule_on_pool(nullptr, h);
}

void schedule_after_on_pool(ThreadPool* pool, std::coroutine_handle<> h, std::chrono::milliseconds delay) {
    if (!pool) {
        pool = current_pool() ? current_pool() : &ThreadPool::global();
    }
    pool->schedule_after(h, delay);
}

} // namespace detail

void ThreadPool::set_default_threads(std::size_t count) noexcept {
    if (count > 0) {
        default_thread_count() = count;
    }
}

ThreadPool& ThreadPool::global() {
    static ThreadPool instance;
    return instance;
}

ThreadPool::ThreadPool()
    : ThreadPool(default_thread_count().load()) {}

ThreadPool::ThreadPool(std::size_t thread_count)
    : ThreadPool(NoThreads{}) {
    // The delegated-to constructor has completed, so if starting a thread throws below the destructor
    // still runs and joins whatever was started.
    if (thread_count == 0) thread_count = 2;

    workers_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back([this](std::stop_token st) { worker_loop(st); });
    }

    timer_thread_ = std::jthread([this](std::stop_token st) { timer_loop(st); });
}

ThreadPool::ThreadPool(NoThreads) noexcept {}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() noexcept {
    // Step 1: stop and join the timer thread first (no new timer is consumed after this point).
    {
        std::lock_guard lock(timer_mtx_);
        timer_thread_.request_stop();
    }
    timer_cv_.notify_all();
    if (timer_thread_.joinable()) timer_thread_.join();

    // Step 2: flush pending timers into the work queue so suspended coroutines are resumed
    // instead of being leaked. Entries cannot be added concurrently any more: schedule_after()
    // sees the stop request under timer_mtx_ and schedules immediately instead.
    {
        std::scoped_lock lock(timer_mtx_, queue_mtx_);
        while (!timers_.empty()) {
            tasks_.push(timers_.top().handle);
            timers_.pop();
        }
    }

    // Step 3: stop and join the workers; they keep draining the queue until it is empty.
    {
        std::lock_guard lock(queue_mtx_);
        for (auto& w : workers_)
            w.request_stop();
    }
    queue_cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

std::size_t ThreadPool::thread_count() const noexcept {
    return workers_.size();
}

void ThreadPool::schedule(std::coroutine_handle<> h) {
    {
        std::lock_guard lock(queue_mtx_);
        // Shutdown path only: once the workers were asked to stop nobody will drain the queue
        // any more, so the handle must run on the calling thread to avoid being stranded.
        if (workers_.empty() || !workers_.front().get_stop_token().stop_requested()) {
            tasks_.push(h);
            queue_cv_.notify_one();
            return;
        }
    }
    h.resume();
}

void ThreadPool::schedule_after(std::coroutine_handle<> h, std::chrono::milliseconds duration) {
    // Clamp BEFORE converting to the clock's finer resolution: adding e.g. milliseconds::max() to a
    // time_point overflows (UB) and would wrap into the past. 100 years is "forever" for a timer.
    constexpr auto max_delay =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(24 * 365 * 100));
    if (duration < std::chrono::milliseconds::zero()) duration = std::chrono::milliseconds::zero();
    if (duration > max_delay) duration = max_delay;

    auto wake_time = std::chrono::steady_clock::now() + duration;
    {
        std::lock_guard lock(timer_mtx_);
        // The check and the push share one lock so no entry can be added after the final flush
        // performed by the destructor.
        if (!timer_thread_.get_stop_token().stop_requested()) {
            timers_.push({wake_time, h});
            timer_cv_.notify_one();
            return;
        }
    }
    schedule(h);
}

void ThreadPool::worker_loop(std::stop_token st) {
    detail::CurrentPoolScope scope{this};

    for (;;) {
        std::coroutine_handle<> task;
        {
            std::unique_lock lock(queue_mtx_);
            queue_cv_.wait(lock, [&] { return st.stop_requested() || !tasks_.empty(); });

            // Keep draining queued work after a stop request; leave only when nothing is left.
            if (st.stop_requested() && tasks_.empty()) return;

            task = tasks_.front();
            tasks_.pop();
        }
        if (task) task.resume();
    }
}

void ThreadPool::timer_loop(std::stop_token st) {
    while (!st.stop_requested()) {
        std::unique_lock lock(timer_mtx_);

        if (timers_.empty()) {
            timer_cv_.wait(lock, [&] { return st.stop_requested() || !timers_.empty(); });
        } else {
            auto next_wake = timers_.top().wake_time;
            timer_cv_.wait_until(lock, next_wake, [&] {
                return st.stop_requested() || (!timers_.empty() && timers_.top().wake_time < next_wake);
            });
        }

        if (st.stop_requested()) return;

        auto now = std::chrono::steady_clock::now();
        while (!timers_.empty() && timers_.top().wake_time <= now) {
            auto h = timers_.top().handle;
            timers_.pop();
            schedule(h);
        }
    }
}

} // namespace aw::tpl::threading
