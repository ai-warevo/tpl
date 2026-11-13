#include <aw/tpl/thread_pool.hpp>

namespace aw::tpl {

namespace detail {

void schedule_on_pool(std::coroutine_handle<> h) {
    ThreadPool* pool = current_pool ? current_pool : &ThreadPool::global();
    pool->schedule(h);
}

} // namespace detail

void ThreadPool::set_default_threads(size_t count) noexcept {
    if (count > 0) {
        default_thread_count_ = count;
    }
}

ThreadPool& ThreadPool::global() {
    static ThreadPool instance(default_thread_count_);
    return instance;
}

ThreadPool::ThreadPool(size_t thread_count) {
    if (thread_count == 0) thread_count = 2;

    workers_.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back([this](std::stop_token st) { worker_loop(st); });
    }

    timer_thread_ = std::jthread([this](std::stop_token st) { timer_loop(st); });
}

ThreadPool::~ThreadPool() {
    timer_thread_.request_stop();
    timer_cv_.notify_all();

    for (auto& w : workers_) w.request_stop();
    queue_cv_.notify_all();
}

size_t ThreadPool::thread_count() const noexcept {
    return workers_.size();
}

void ThreadPool::schedule(std::coroutine_handle<> h) {
    {
        std::lock_guard lock(queue_mtx_);
        tasks_.push(h);
    }
    queue_cv_.notify_one();
}

void ThreadPool::schedule_after(std::coroutine_handle<> h, std::chrono::milliseconds duration) {
    auto wake_time = std::chrono::steady_clock::now() + duration;
    {
        std::lock_guard lock(timer_mtx_);
        timers_.push({wake_time, h});
    }
    timer_cv_.notify_one();
}

void ThreadPool::worker_loop(std::stop_token st) {
    struct PoolGuard {
        PoolGuard(ThreadPool* p) noexcept { detail::current_pool = p; }
        ~PoolGuard() noexcept { detail::current_pool = nullptr; }
    } guard{this};

    while (!st.stop_requested()) {
        std::coroutine_handle<> task;
        {
            std::unique_lock lock(queue_mtx_);
            queue_cv_.wait(lock, [&] { return st.stop_requested() || !tasks_.empty(); });

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

} // namespace aw::tpl
