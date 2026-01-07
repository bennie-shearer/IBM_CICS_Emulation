#pragma once
// =============================================================================
// CICS Emulation - Threading and Concurrency (C++20)
// Version: 3.4.6
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <thread>
#include <stop_token>
#include <queue>
#include <condition_variable>
#include <future>
#include <numeric>

namespace cics::threading {

using namespace cics;

// =============================================================================
// Thread Pool Configuration
// =============================================================================
struct ThreadPoolConfig {
    Size min_threads = 2;
    Size max_threads = std::thread::hardware_concurrency();
    Size queue_size = 1000;
    Milliseconds idle_timeout{60000};
    bool work_stealing = false;
};

// =============================================================================
// Task Priority
// =============================================================================
enum class TaskPriority : UInt8 {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// =============================================================================
// Thread Pool
// =============================================================================
class ThreadPool {
public:
    struct Statistics {
        UInt64 completed_tasks;
        UInt64 rejected_tasks;
        UInt64 active_tasks;
    };
    
private:
    struct Task {
        std::function<void()> func;
        TaskPriority priority = TaskPriority::NORMAL;
        
        Task() = default;
        Task(std::function<void()> f, TaskPriority p) : func(std::move(f)), priority(p) {}
        
        bool operator<(const Task& other) const {
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
        
        void operator()() const { if (func) func(); }
    };
    
    ThreadPoolConfig config_;
    std::vector<std::jthread> workers_;
    std::priority_queue<Task> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> shutdown_{false};
    AtomicCounter<> active_tasks_;
    AtomicCounter<> completed_tasks_;
    AtomicCounter<> rejected_tasks_;
    
    void worker_loop(std::stop_token stop_token);
    
public:
    explicit ThreadPool(ThreadPoolConfig config = {});
    ~ThreadPool();
    
    void execute(std::function<void()> task, TaskPriority priority = TaskPriority::NORMAL);
    
    template<typename F, typename... Args>
    auto submit(F&& func, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using result_type = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<result_type()>>(
            std::bind(std::forward<F>(func), std::forward<Args>(args)...)
        );
        auto future = task->get_future();
        execute([task]() { (*task)(); });
        return future;
    }
    
    void wait_all();
    void shutdown();
    
    [[nodiscard]] Size queue_size() const;
    [[nodiscard]] Size active_count() const { return active_tasks_.get(); }
    [[nodiscard]] bool is_shutdown() const { return shutdown_; }
    
    [[nodiscard]] Statistics statistics() const {
        return {completed_tasks_.get(), rejected_tasks_.get(), active_tasks_.get()};
    }
};

// =============================================================================
// Read-Write Lock
// =============================================================================
class ReadWriteLock {
private:
    std::shared_mutex mutex_;
    
public:
    void lock_read() { mutex_.lock_shared(); }
    void unlock_read() { mutex_.unlock_shared(); }
    void lock_write() { mutex_.lock(); }
    void unlock_write() { mutex_.unlock(); }
    
    [[nodiscard]] std::shared_lock<std::shared_mutex> read_guard() {
        return std::shared_lock{mutex_};
    }
    
    [[nodiscard]] std::unique_lock<std::shared_mutex> write_guard() {
        return std::unique_lock{mutex_};
    }
};

// =============================================================================
// Spin Lock
// =============================================================================
class SpinLock {
private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    
public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            while (flag_.test(std::memory_order_relaxed)) {
                std::this_thread::yield();
            }
        }
    }
    
    bool try_lock() {
        return !flag_.test_and_set(std::memory_order_acquire);
    }
    
    void unlock() {
        flag_.clear(std::memory_order_release);
    }
};

// =============================================================================
// Concurrent Queue
// =============================================================================
template<typename T>
class ConcurrentQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
public:
    void push(T item) {
        std::lock_guard lock(mutex_);
        queue_.push(std::move(item));
        cv_.notify_one();
    }
    
    bool try_pop(T& item) {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    template<typename Rep, typename Period>
    Optional<T> wait_pop(std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this]() { return !queue_.empty(); })) {
            return nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }
    
    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }
    
    [[nodiscard]] Size size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }
};

// =============================================================================
// Global Thread Pool
// =============================================================================
ThreadPool& global_thread_pool();
void set_global_thread_pool(UniquePtr<ThreadPool> pool);
void shutdown_global_thread_pool();

template<typename F, typename... Args>
auto async(F&& func, Args&&... args) {
    return global_thread_pool().submit(std::forward<F>(func), std::forward<Args>(args)...);
}

// =============================================================================
// Parallel Algorithms
// =============================================================================
template<typename Iterator, typename Func>
void parallel_for(Iterator begin, Iterator end, Func func, Size chunk_size = 0) {
    auto total = std::distance(begin, end);
    if (total <= 0) return;
    
    if (chunk_size == 0) {
        chunk_size = std::max(Size(1), static_cast<Size>(total) / 
                             (std::thread::hardware_concurrency() * 4));
    }
    
    std::vector<std::future<void>> futures;
    auto it = begin;
    
    while (it != end) {
        auto chunk_end = it;
        std::advance(chunk_end, std::min(static_cast<Size>(std::distance(it, end)), chunk_size));
        
        futures.push_back(global_thread_pool().submit([it, chunk_end, &func]() {
            for (auto i = it; i != chunk_end; ++i) {
                func(*i);
            }
        }));
        
        it = chunk_end;
    }
    
    for (auto& f : futures) f.wait();
}

template<typename Iterator, typename T, typename BinaryOp>
T parallel_reduce(Iterator begin, Iterator end, T init, BinaryOp op, Size chunk_size = 0) {
    auto total = std::distance(begin, end);
    if (total <= 0) return init;
    
    if (chunk_size == 0) {
        chunk_size = std::max(Size(1), static_cast<Size>(total) / 
                             std::thread::hardware_concurrency());
    }
    
    std::vector<std::future<T>> futures;
    auto it = begin;
    
    while (it != end) {
        auto chunk_end = it;
        std::advance(chunk_end, std::min(static_cast<Size>(std::distance(it, end)), chunk_size));
        
        futures.push_back(global_thread_pool().submit([it, chunk_end, init, &op]() {
            return std::accumulate(it, chunk_end, init, op);
        }));
        
        it = chunk_end;
    }
    
    T result = init;
    for (auto& f : futures) {
        result = op(result, f.get());
    }
    return result;
}

} // namespace cics::threading
