// =============================================================================
// CICS Emulation - Task Control Module
// Version: 3.4.6
// =============================================================================
// Implements CICS Task Control services:
// - ENQ: Enqueue (lock) a resource
// - DEQ: Dequeue (unlock) a resource
// - SUSPEND: Suspend current task
// - Task management and serialization
// =============================================================================

#ifndef CICS_TASK_CONTROL_HPP
#define CICS_TASK_CONTROL_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>
#include <set>
#include <chrono>
#include <thread>
#include <atomic>
#include <queue>

namespace cics {
namespace task {

// =============================================================================
// Resource Lock Types
// =============================================================================

enum class LockType : UInt8 {
    EXCLUSIVE,  // Single owner (default for ENQ)
    SHARED,     // Multiple readers allowed
    UPDATE      // Upgrade from shared to exclusive
};

enum class LockScope : UInt8 {
    TASK,       // Released when task ends
    UOW,        // Released at sync point
    PERMANENT   // Explicit DEQ required
};

enum class WaitOption : UInt8 {
    WAIT,       // Wait for resource (default)
    NOWAIT,     // Return immediately if busy
    NOSUSPEND   // Same as NOWAIT
};

// =============================================================================
// Resource Definition
// =============================================================================

struct ResourceId {
    String name;
    UInt32 length = 0;  // For length-based resources
    
    bool operator==(const ResourceId& other) const {
        return name == other.name && length == other.length;
    }
    
    [[nodiscard]] String to_string() const;
};

struct ResourceIdHash {
    std::size_t operator()(const ResourceId& id) const {
        return std::hash<String>{}(id.name) ^ (std::hash<UInt32>{}(id.length) << 1);
    }
};

// =============================================================================
// Lock Request
// =============================================================================

struct LockRequest {
    ResourceId resource;
    LockType type = LockType::EXCLUSIVE;
    LockScope scope = LockScope::TASK;
    WaitOption wait = WaitOption::WAIT;
    UInt32 task_id = 0;
    std::chrono::steady_clock::time_point request_time;
    std::chrono::milliseconds max_wait{0};  // 0 = infinite
    
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// Lock Entry (internal)
// =============================================================================

struct LockEntry {
    ResourceId resource;
    LockType type = LockType::EXCLUSIVE;
    std::set<UInt32> owners;  // Task IDs holding the lock
    std::queue<LockRequest> waiters;  // Tasks waiting for lock
    UInt32 exclusive_owner = 0;  // For exclusive locks
    std::chrono::steady_clock::time_point acquired_time;
    
    [[nodiscard]] bool is_held() const { return !owners.empty() || exclusive_owner != 0; }
    [[nodiscard]] bool is_exclusive() const { return exclusive_owner != 0; }
    [[nodiscard]] UInt32 owner_count() const { 
        return exclusive_owner ? 1 : static_cast<UInt32>(owners.size()); 
    }
};

// =============================================================================
// Task State
// =============================================================================

enum class TaskState : UInt8 {
    RUNNING,
    SUSPENDED,
    WAITING,
    TERMINATED
};

struct TaskInfo {
    UInt32 task_id = 0;
    FixedString<4> transaction_id;
    TaskState state = TaskState::RUNNING;
    std::thread::id thread_id;
    std::set<ResourceId, std::less<>> held_resources;
    std::chrono::steady_clock::time_point start_time;
    UInt32 priority = 0;
    String user_id;
    
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// Task Control Manager
// =============================================================================

class TaskControlManager {
private:
    mutable std::shared_mutex mutex_;
    std::condition_variable_any cv_;
    
    // Lock management
    std::unordered_map<ResourceId, LockEntry, ResourceIdHash> locks_;
    
    // Task management
    std::unordered_map<UInt32, TaskInfo> tasks_;
    UInt32 next_task_id_ = 1;
    thread_local static UInt32 current_task_id_;
    
    // Statistics
    struct Statistics {
        UInt64 enq_count = 0;
        UInt64 deq_count = 0;
        UInt64 enq_wait_count = 0;
        UInt64 enq_nowait_fails = 0;
        UInt64 deadlock_detections = 0;
        UInt64 max_waiters = 0;
        UInt64 suspend_count = 0;
    } stats_;
    
    // Deadlock detection
    bool detect_deadlock(UInt32 task_id, const ResourceId& resource);
    void cleanup_task_locks(UInt32 task_id);
    
public:
    TaskControlManager() = default;
    ~TaskControlManager();
    
    // Singleton access
    static TaskControlManager& instance();
    
    // Task lifecycle
    Result<UInt32> create_task(const FixedString<4>& transid);
    Result<void> end_task(UInt32 task_id);
    Result<void> end_current_task();
    [[nodiscard]] UInt32 get_current_task_id() const;
    void set_current_task_id(UInt32 task_id);
    Result<TaskInfo*> get_task(UInt32 task_id);
    
    // ENQ - Enqueue resource
    Result<void> enq(const ResourceId& resource, LockType type = LockType::EXCLUSIVE,
                     LockScope scope = LockScope::TASK, WaitOption wait = WaitOption::WAIT);
    Result<void> enq(StringView resource_name, LockType type = LockType::EXCLUSIVE);
    Result<void> enq(StringView resource_name, UInt32 length, LockType type = LockType::EXCLUSIVE);
    
    // DEQ - Dequeue resource
    Result<void> deq(const ResourceId& resource);
    Result<void> deq(StringView resource_name);
    Result<void> deq(StringView resource_name, UInt32 length);
    Result<void> deq_all();  // Release all resources for current task
    
    // SUSPEND - Suspend task
    Result<void> suspend();
    Result<void> suspend(std::chrono::milliseconds duration);
    
    // Query operations
    [[nodiscard]] bool is_locked(const ResourceId& resource) const;
    [[nodiscard]] bool is_locked(StringView resource_name) const;
    [[nodiscard]] bool owns_lock(const ResourceId& resource) const;
    [[nodiscard]] UInt32 lock_count() const;
    [[nodiscard]] UInt32 task_count() const;
    [[nodiscard]] std::vector<TaskInfo> list_tasks() const;
    [[nodiscard]] std::vector<ResourceId> list_locks() const;
    
    // Statistics
    [[nodiscard]] String get_statistics() const;
    void reset_statistics();
};

// =============================================================================
// EXEC CICS Style Interface
// =============================================================================

Result<void> exec_cics_enq(StringView resource);
Result<void> exec_cics_enq(StringView resource, UInt32 length);
Result<void> exec_cics_enq_nosuspend(StringView resource);
Result<void> exec_cics_deq(StringView resource);
Result<void> exec_cics_deq(StringView resource, UInt32 length);
Result<void> exec_cics_suspend();

// =============================================================================
// RAII Lock Guard
// =============================================================================

class ResourceLock {
private:
    ResourceId resource_;
    bool locked_ = false;
    
public:
    ResourceLock(StringView resource_name, LockType type = LockType::EXCLUSIVE);
    ResourceLock(const ResourceId& resource, LockType type = LockType::EXCLUSIVE);
    ~ResourceLock();
    
    ResourceLock(const ResourceLock&) = delete;
    ResourceLock& operator=(const ResourceLock&) = delete;
    ResourceLock(ResourceLock&& other) noexcept;
    ResourceLock& operator=(ResourceLock&& other) noexcept;
    
    [[nodiscard]] bool is_locked() const { return locked_; }
    void unlock();
};

} // namespace task
} // namespace cics

#endif // CICS_TASK_CONTROL_HPP
