#include "cics/common/threading.hpp"

namespace cics::threading {

ThreadPool::ThreadPool(ThreadPoolConfig config) : config_(std::move(config)) {
    Size num_threads = std::clamp(config_.max_threads, config_.min_threads, 
                                   static_cast<Size>(std::thread::hardware_concurrency() * 2));
    workers_.reserve(num_threads);
    for (Size i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this](std::stop_token token) { worker_loop(token); });
    }
}

ThreadPool::~ThreadPool() { shutdown(); }

void ThreadPool::execute(std::function<void()> task, TaskPriority priority) {
    {
        std::lock_guard lock(queue_mutex_);
        if (shutdown_) { rejected_tasks_++; return; }
        if (tasks_.size() >= config_.queue_size) { rejected_tasks_++; return; }
        tasks_.emplace(std::move(task), priority);
    }
    cv_.notify_one();
}

void ThreadPool::wait_all() {
    std::unique_lock lock(queue_mutex_);
    cv_.wait(lock, [this]() { return tasks_.empty() && active_tasks_ == 0; });
}

void ThreadPool::shutdown() {
    {
        std::lock_guard lock(queue_mutex_);
        shutdown_ = true;
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
        worker.request_stop();
        if (worker.joinable()) worker.join();
    }
}

Size ThreadPool::queue_size() const {
    std::lock_guard lock(queue_mutex_);
    return tasks_.size();
}

void ThreadPool::worker_loop(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        Task task;
        {
            std::unique_lock lock(queue_mutex_);
            cv_.wait(lock, [this, &stop_token]() {
                return !tasks_.empty() || shutdown_ || stop_token.stop_requested();
            });
            if ((shutdown_ || stop_token.stop_requested()) && tasks_.empty()) return;
            if (tasks_.empty()) continue;
            task = std::move(const_cast<Task&>(tasks_.top()));
            tasks_.pop();
        }
        active_tasks_++;
        try { task(); } catch (...) {}
        active_tasks_--;
        completed_tasks_++;
        cv_.notify_all();
    }
}

static UniquePtr<ThreadPool> g_thread_pool;
static std::once_flag g_pool_init;

ThreadPool& global_thread_pool() {
    std::call_once(g_pool_init, []() {
        g_thread_pool = make_unique<ThreadPool>();
    });
    return *g_thread_pool;
}

void set_global_thread_pool(UniquePtr<ThreadPool> pool) {
    g_thread_pool = std::move(pool);
}

void shutdown_global_thread_pool() {
    if (g_thread_pool) g_thread_pool->shutdown();
}

} // namespace cics::threading
