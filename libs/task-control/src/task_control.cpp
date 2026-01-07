// =============================================================================
// CICS Emulation - Task Control Module Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/task/task_control.hpp>
#include <sstream>
#include <algorithm>

namespace cics {
namespace task {

// Thread-local current task ID
thread_local UInt32 TaskControlManager::current_task_id_ = 0;

// =============================================================================
// ResourceId Implementation
// =============================================================================

String ResourceId::to_string() const {
    std::ostringstream oss;
    oss << "Resource{" << name;
    if (length > 0) {
        oss << ", len=" << length;
    }
    oss << "}";
    return oss.str();
}

// Comparison operator for std::set
bool operator<(const ResourceId& a, const ResourceId& b) {
    if (a.name != b.name) return a.name < b.name;
    return a.length < b.length;
}

// =============================================================================
// LockRequest Implementation
// =============================================================================

String LockRequest::to_string() const {
    std::ostringstream oss;
    oss << "LockRequest{resource=" << resource.to_string()
        << ", task=" << task_id
        << ", type=" << (type == LockType::EXCLUSIVE ? "EXCL" : 
                        type == LockType::SHARED ? "SHR" : "UPD")
        << "}";
    return oss.str();
}

// =============================================================================
// TaskInfo Implementation
// =============================================================================

String TaskInfo::to_string() const {
    std::ostringstream oss;
    oss << "Task{id=" << task_id
        << ", trans=" << String(transaction_id.data(), 4)
        << ", state=" << static_cast<int>(state)
        << ", resources=" << held_resources.size()
        << "}";
    return oss.str();
}

// =============================================================================
// TaskControlManager Implementation
// =============================================================================

TaskControlManager::~TaskControlManager() {
    // Clean up all tasks
    std::unique_lock<std::shared_mutex> lock(mutex_);
    tasks_.clear();
    locks_.clear();
}

TaskControlManager& TaskControlManager::instance() {
    static TaskControlManager instance;
    return instance;
}

Result<UInt32> TaskControlManager::create_task(const FixedString<4>& transid) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    UInt32 task_id = next_task_id_++;
    
    TaskInfo task;
    task.task_id = task_id;
    task.transaction_id = transid;
    task.state = TaskState::RUNNING;
    task.thread_id = std::this_thread::get_id();
    task.start_time = std::chrono::steady_clock::now();
    
    tasks_[task_id] = task;
    current_task_id_ = task_id;
    
    return make_success(task_id);
}

Result<void> TaskControlManager::end_task(UInt32 task_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Task not found: " + std::to_string(task_id));
    }
    
    // Release all locks held by this task
    cleanup_task_locks(task_id);
    
    it->second.state = TaskState::TERMINATED;
    tasks_.erase(it);
    
    if (current_task_id_ == task_id) {
        current_task_id_ = 0;
    }
    
    return make_success();
}

Result<void> TaskControlManager::end_current_task() {
    return end_task(current_task_id_);
}

UInt32 TaskControlManager::get_current_task_id() const {
    return current_task_id_;
}

void TaskControlManager::set_current_task_id(UInt32 task_id) {
    current_task_id_ = task_id;
}

Result<TaskInfo*> TaskControlManager::get_task(UInt32 task_id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return make_error<TaskInfo*>(ErrorCode::RECORD_NOT_FOUND,
            "Task not found: " + std::to_string(task_id));
    }
    
    return make_success(&it->second);
}

Result<void> TaskControlManager::enq(const ResourceId& resource, LockType type,
                                      LockScope scope, WaitOption wait) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    ++stats_.enq_count;
    
    UInt32 task_id = current_task_id_;
    if (task_id == 0) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "No active task");
    }
    
    auto& lock_entry = locks_[resource];
    lock_entry.resource = resource;
    
    // Check if we already own this lock
    if (type == LockType::EXCLUSIVE && lock_entry.exclusive_owner == task_id) {
        return make_success(); // Already own exclusive
    }
    if (type == LockType::SHARED && lock_entry.owners.count(task_id)) {
        return make_success(); // Already own shared
    }
    
    // Try to acquire the lock
    auto try_acquire = [&]() -> bool {
        if (type == LockType::EXCLUSIVE) {
            if (!lock_entry.is_held()) {
                lock_entry.exclusive_owner = task_id;
                lock_entry.type = LockType::EXCLUSIVE;
                lock_entry.acquired_time = std::chrono::steady_clock::now();
                return true;
            }
        } else if (type == LockType::SHARED) {
            if (!lock_entry.is_exclusive()) {
                lock_entry.owners.insert(task_id);
                lock_entry.type = LockType::SHARED;
                if (lock_entry.owners.size() == 1) {
                    lock_entry.acquired_time = std::chrono::steady_clock::now();
                }
                return true;
            }
        }
        return false;
    };
    
    if (try_acquire()) {
        // Add to task's held resources
        auto task_it = tasks_.find(task_id);
        if (task_it != tasks_.end()) {
            task_it->second.held_resources.insert(resource);
        }
        return make_success();
    }
    
    // Lock is busy
    if (wait == WaitOption::NOWAIT || wait == WaitOption::NOSUSPEND) {
        ++stats_.enq_nowait_fails;
        return make_error<void>(ErrorCode::RESOURCE_EXHAUSTED, 
            "Resource is locked: " + resource.to_string());
    }
    
    // Check for deadlock
    if (detect_deadlock(task_id, resource)) {
        ++stats_.deadlock_detections;
        return make_error<void>(ErrorCode::TIMEOUT, 
            "Deadlock detected for resource: " + resource.to_string());
    }
    
    // Add to waiters queue
    ++stats_.enq_wait_count;
    LockRequest request;
    request.resource = resource;
    request.type = type;
    request.scope = scope;
    request.task_id = task_id;
    request.request_time = std::chrono::steady_clock::now();
    
    lock_entry.waiters.push(request);
    stats_.max_waiters = std::max(stats_.max_waiters, 
        static_cast<UInt64>(lock_entry.waiters.size()));
    
    // Update task state
    auto task_it = tasks_.find(task_id);
    if (task_it != tasks_.end()) {
        task_it->second.state = TaskState::WAITING;
    }
    
    // Wait for the lock
    cv_.wait(lock, [&]() {
        return try_acquire() || 
               (task_it != tasks_.end() && task_it->second.state == TaskState::TERMINATED);
    });
    
    if (task_it != tasks_.end()) {
        task_it->second.state = TaskState::RUNNING;
        task_it->second.held_resources.insert(resource);
    }
    
    return make_success();
}

Result<void> TaskControlManager::enq(StringView resource_name, LockType type) {
    ResourceId resource;
    resource.name = String(resource_name);
    return enq(resource, type);
}

Result<void> TaskControlManager::enq(StringView resource_name, UInt32 length, LockType type) {
    ResourceId resource;
    resource.name = String(resource_name);
    resource.length = length;
    return enq(resource, type);
}

Result<void> TaskControlManager::deq(const ResourceId& resource) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    ++stats_.deq_count;
    
    UInt32 task_id = current_task_id_;
    if (task_id == 0) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "No active task");
    }
    
    auto it = locks_.find(resource);
    if (it == locks_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Resource not locked: " + resource.to_string());
    }
    
    auto& lock_entry = it->second;
    
    // Check ownership and release
    if (lock_entry.exclusive_owner == task_id) {
        lock_entry.exclusive_owner = 0;
    } else if (lock_entry.owners.count(task_id)) {
        lock_entry.owners.erase(task_id);
    } else {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT,
            "Task does not own this lock");
    }
    
    // Remove from task's held resources
    auto task_it = tasks_.find(task_id);
    if (task_it != tasks_.end()) {
        task_it->second.held_resources.erase(resource);
    }
    
    // Wake up waiters if lock is free
    if (!lock_entry.is_held() && !lock_entry.waiters.empty()) {
        cv_.notify_all();
    }
    
    // Clean up if completely released
    if (!lock_entry.is_held() && lock_entry.waiters.empty()) {
        locks_.erase(it);
    }
    
    return make_success();
}

Result<void> TaskControlManager::deq(StringView resource_name) {
    ResourceId resource;
    resource.name = String(resource_name);
    return deq(resource);
}

Result<void> TaskControlManager::deq(StringView resource_name, UInt32 length) {
    ResourceId resource;
    resource.name = String(resource_name);
    resource.length = length;
    return deq(resource);
}

Result<void> TaskControlManager::deq_all() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    UInt32 task_id = current_task_id_;
    if (task_id == 0) {
        return make_success();
    }
    
    cleanup_task_locks(task_id);
    return make_success();
}

Result<void> TaskControlManager::suspend() {
    ++stats_.suspend_count;
    
    UInt32 task_id = current_task_id_;
    if (task_id == 0) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "No active task");
    }
    
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = tasks_.find(task_id);
        if (it != tasks_.end()) {
            it->second.state = TaskState::SUSPENDED;
        }
    }
    
    std::this_thread::yield();
    
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = tasks_.find(task_id);
        if (it != tasks_.end()) {
            it->second.state = TaskState::RUNNING;
        }
    }
    
    return make_success();
}

Result<void> TaskControlManager::suspend(std::chrono::milliseconds duration) {
    ++stats_.suspend_count;
    
    UInt32 task_id = current_task_id_;
    if (task_id == 0) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "No active task");
    }
    
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = tasks_.find(task_id);
        if (it != tasks_.end()) {
            it->second.state = TaskState::SUSPENDED;
        }
    }
    
    std::this_thread::sleep_for(duration);
    
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = tasks_.find(task_id);
        if (it != tasks_.end()) {
            it->second.state = TaskState::RUNNING;
        }
    }
    
    return make_success();
}

bool TaskControlManager::is_locked(const ResourceId& resource) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = locks_.find(resource);
    return it != locks_.end() && it->second.is_held();
}

bool TaskControlManager::is_locked(StringView resource_name) const {
    ResourceId resource;
    resource.name = String(resource_name);
    return is_locked(resource);
}

bool TaskControlManager::owns_lock(const ResourceId& resource) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    UInt32 task_id = current_task_id_;
    if (task_id == 0) return false;
    
    auto it = locks_.find(resource);
    if (it == locks_.end()) return false;
    
    return it->second.exclusive_owner == task_id || 
           it->second.owners.count(task_id) > 0;
}

UInt32 TaskControlManager::lock_count() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return static_cast<UInt32>(locks_.size());
}

UInt32 TaskControlManager::task_count() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return static_cast<UInt32>(tasks_.size());
}

std::vector<TaskInfo> TaskControlManager::list_tasks() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<TaskInfo> result;
    for (const auto& [id, task] : tasks_) {
        result.push_back(task);
    }
    return result;
}

std::vector<ResourceId> TaskControlManager::list_locks() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<ResourceId> result;
    for (const auto& [id, lock_entry] : locks_) {
        if (lock_entry.is_held()) {
            result.push_back(id);
        }
    }
    return result;
}

bool TaskControlManager::detect_deadlock(UInt32 task_id, const ResourceId& resource) {
    // Simple deadlock detection: check if resource owner is waiting for our resources
    auto it = locks_.find(resource);
    if (it == locks_.end()) return false;
    
    UInt32 owner = it->second.exclusive_owner;
    if (owner == 0 && !it->second.owners.empty()) {
        owner = *it->second.owners.begin();
    }
    if (owner == 0 || owner == task_id) return false;
    
    // Check if owner is waiting for any resource we hold
    auto task_it = tasks_.find(task_id);
    if (task_it == tasks_.end()) return false;
    
    for (const auto& held : task_it->second.held_resources) {
        auto lock_it = locks_.find(held);
        if (lock_it != locks_.end()) {
            auto& waiters = lock_it->second.waiters;
            // Check waiters queue
            std::queue<LockRequest> temp = waiters;
            while (!temp.empty()) {
                if (temp.front().task_id == owner) {
                    return true; // Deadlock!
                }
                temp.pop();
            }
        }
    }
    
    return false;
}

void TaskControlManager::cleanup_task_locks(UInt32 task_id) {
    // Must be called with mutex_ held
    std::vector<ResourceId> to_remove;
    
    for (auto& [id, lock_entry] : locks_) {
        bool was_held = false;
        
        if (lock_entry.exclusive_owner == task_id) {
            lock_entry.exclusive_owner = 0;
            was_held = true;
        }
        lock_entry.owners.erase(task_id);
        
        // Remove from waiters
        std::queue<LockRequest> new_waiters;
        while (!lock_entry.waiters.empty()) {
            auto req = lock_entry.waiters.front();
            lock_entry.waiters.pop();
            if (req.task_id != task_id) {
                new_waiters.push(req);
            }
        }
        lock_entry.waiters = std::move(new_waiters);
        
        if (!lock_entry.is_held() && lock_entry.waiters.empty()) {
            to_remove.push_back(id);
        } else if (was_held) {
            cv_.notify_all();
        }
    }
    
    for (const auto& id : to_remove) {
        locks_.erase(id);
    }
}

String TaskControlManager::get_statistics() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "Task Control Statistics:\n"
        << "  ENQ calls:           " << stats_.enq_count << "\n"
        << "  DEQ calls:           " << stats_.deq_count << "\n"
        << "  ENQ waits:           " << stats_.enq_wait_count << "\n"
        << "  NOWAIT failures:     " << stats_.enq_nowait_fails << "\n"
        << "  Deadlocks detected:  " << stats_.deadlock_detections << "\n"
        << "  Max waiters:         " << stats_.max_waiters << "\n"
        << "  SUSPEND calls:       " << stats_.suspend_count << "\n"
        << "  Active tasks:        " << tasks_.size() << "\n"
        << "  Active locks:        " << locks_.size() << "\n";
    
    return oss.str();
}

void TaskControlManager::reset_statistics() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    stats_ = Statistics{};
}

// =============================================================================
// EXEC CICS Interface Implementation
// =============================================================================

Result<void> exec_cics_enq(StringView resource) {
    return TaskControlManager::instance().enq(resource);
}

Result<void> exec_cics_enq(StringView resource, UInt32 length) {
    return TaskControlManager::instance().enq(resource, length);
}

Result<void> exec_cics_enq_nosuspend(StringView resource) {
    ResourceId res;
    res.name = String(resource);
    return TaskControlManager::instance().enq(res, LockType::EXCLUSIVE, 
        LockScope::TASK, WaitOption::NOSUSPEND);
}

Result<void> exec_cics_deq(StringView resource) {
    return TaskControlManager::instance().deq(resource);
}

Result<void> exec_cics_deq(StringView resource, UInt32 length) {
    return TaskControlManager::instance().deq(resource, length);
}

Result<void> exec_cics_suspend() {
    return TaskControlManager::instance().suspend();
}

// =============================================================================
// ResourceLock Implementation
// =============================================================================

ResourceLock::ResourceLock(StringView resource_name, LockType type) {
    resource_.name = String(resource_name);
    auto result = TaskControlManager::instance().enq(resource_, type);
    locked_ = result.is_success();
}

ResourceLock::ResourceLock(const ResourceId& resource, LockType type) 
    : resource_(resource) {
    auto result = TaskControlManager::instance().enq(resource_, type);
    locked_ = result.is_success();
}

ResourceLock::~ResourceLock() {
    if (locked_) {
        TaskControlManager::instance().deq(resource_);
    }
}

ResourceLock::ResourceLock(ResourceLock&& other) noexcept 
    : resource_(std::move(other.resource_)), locked_(other.locked_) {
    other.locked_ = false;
}

ResourceLock& ResourceLock::operator=(ResourceLock&& other) noexcept {
    if (this != &other) {
        if (locked_) {
            TaskControlManager::instance().deq(resource_);
        }
        resource_ = std::move(other.resource_);
        locked_ = other.locked_;
        other.locked_ = false;
    }
    return *this;
}

void ResourceLock::unlock() {
    if (locked_) {
        TaskControlManager::instance().deq(resource_);
        locked_ = false;
    }
}

} // namespace task
} // namespace cics
