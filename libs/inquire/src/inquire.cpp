// =============================================================================
// CICS Emulation - Inquire Services Implementation
// =============================================================================

#include <cics/inquire/inquire.hpp>
#include <algorithm>

namespace cics {
namespace inquire {

// =============================================================================
// Utility Functions
// =============================================================================

String resource_type_to_string(ResourceType type) {
    switch (type) {
        case ResourceType::PROGRAM: return "PROGRAM";
        case ResourceType::FILE: return "FILE";
        case ResourceType::TRANSACTION: return "TRANSACTION";
        case ResourceType::TERMINAL: return "TERMINAL";
        case ResourceType::TDQUEUE: return "TDQUEUE";
        case ResourceType::TSQUEUE: return "TSQUEUE";
        case ResourceType::CONNECTION: return "CONNECTION";
        case ResourceType::TASK: return "TASK";
        case ResourceType::SYSTEM: return "SYSTEM";
        default: return "UNKNOWN";
    }
}

String resource_status_to_string(ResourceStatus status) {
    switch (status) {
        case ResourceStatus::ENABLED: return "ENABLED";
        case ResourceStatus::DISABLED: return "DISABLED";
        case ResourceStatus::UNENABLED: return "UNENABLED";
        case ResourceStatus::CLOSED: return "CLOSED";
        case ResourceStatus::OPEN: return "OPEN";
        case ResourceStatus::ACTIVE: return "ACTIVE";
        case ResourceStatus::SUSPENDED: return "SUSPENDED";
        case ResourceStatus::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

String enable_status_to_string(EnableStatus status) {
    switch (status) {
        case EnableStatus::ENABLED: return "ENABLED";
        case EnableStatus::DISABLED: return "DISABLED";
        case EnableStatus::PENDING: return "PENDING";
        default: return "UNKNOWN";
    }
}

String open_status_to_string(OpenStatus status) {
    switch (status) {
        case OpenStatus::OPEN: return "OPEN";
        case OpenStatus::CLOSED: return "CLOSED";
        case OpenStatus::CLOSING: return "CLOSING";
        case OpenStatus::OPENING: return "OPENING";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// InquireManager Implementation
// =============================================================================

InquireManager& InquireManager::instance() {
    static InquireManager instance;
    return instance;
}

void InquireManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;
    
    programs_.clear();
    files_.clear();
    transactions_.clear();
    terminals_.clear();
    tdqueues_.clear();
    tsqueues_.clear();
    tasks_.clear();
    
    // Initialize system info
    system_info_.applid = "CICSPROD";
    system_info_.release = "3.4.6";
    system_info_.version = "CICS Emulation";
    system_info_.startup_time = std::chrono::system_clock::now();
    system_info_.max_tasks = 999;
    system_info_.current_tasks = 0;
    system_info_.transactions_completed = 0;
    system_info_.status = "ACTIVE";
    
    initialized_ = true;
}

void InquireManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    programs_.clear();
    files_.clear();
    transactions_.clear();
    terminals_.clear();
    tdqueues_.clear();
    tsqueues_.clear();
    tasks_.clear();
    initialized_ = false;
}

Result<ProgramInfo> InquireManager::inquire_program(StringView name) {
    auto info = programs_.get(name);
    if (!info.has_value()) {
        return make_error<ProgramInfo>(ErrorCode::CICS_PROGRAM_NOT_FOUND,
            "Program not found: " + String(name));
    }
    return make_success(info.value());
}

std::vector<ProgramInfo> InquireManager::inquire_all_programs() {
    return programs_.list_all();
}

void InquireManager::register_program(const ProgramInfo& info) {
    programs_.register_resource(info.name, info);
}

Result<FileInfo> InquireManager::inquire_file(StringView name) {
    auto info = files_.get(name);
    if (!info.has_value()) {
        return make_error<FileInfo>(ErrorCode::CICS_FILE_NOT_FOUND,
            "File not found: " + String(name));
    }
    return make_success(info.value());
}

std::vector<FileInfo> InquireManager::inquire_all_files() {
    return files_.list_all();
}

void InquireManager::register_file(const FileInfo& info) {
    files_.register_resource(info.name, info);
}

Result<TransactionInfo> InquireManager::inquire_transaction(StringView name) {
    auto info = transactions_.get(name);
    if (!info.has_value()) {
        return make_error<TransactionInfo>(ErrorCode::CICS_TRANSACTION_NOT_FOUND,
            "Transaction not found: " + String(name));
    }
    return make_success(info.value());
}

std::vector<TransactionInfo> InquireManager::inquire_all_transactions() {
    return transactions_.list_all();
}

void InquireManager::register_transaction(const TransactionInfo& info) {
    transactions_.register_resource(info.name, info);
}

Result<TerminalInfo> InquireManager::inquire_terminal(StringView id) {
    auto info = terminals_.get(id);
    if (!info.has_value()) {
        return make_error<TerminalInfo>(ErrorCode::CICS_TERMINAL_NOT_FOUND,
            "Terminal not found: " + String(id));
    }
    return make_success(info.value());
}

std::vector<TerminalInfo> InquireManager::inquire_all_terminals() {
    return terminals_.list_all();
}

void InquireManager::register_terminal(const TerminalInfo& info) {
    terminals_.register_resource(info.id, info);
}

Result<TDQueueInfo> InquireManager::inquire_tdqueue(StringView name) {
    auto info = tdqueues_.get(name);
    if (!info.has_value()) {
        return make_error<TDQueueInfo>(ErrorCode::CICS_QUEUE_NOT_FOUND,
            "TDQueue not found: " + String(name));
    }
    return make_success(info.value());
}

std::vector<TDQueueInfo> InquireManager::inquire_all_tdqueues() {
    return tdqueues_.list_all();
}

void InquireManager::register_tdqueue(const TDQueueInfo& info) {
    tdqueues_.register_resource(info.name, info);
}

Result<TSQueueInfo> InquireManager::inquire_tsqueue(StringView name) {
    auto info = tsqueues_.get(name);
    if (!info.has_value()) {
        return make_error<TSQueueInfo>(ErrorCode::CICS_QUEUE_NOT_FOUND,
            "TSQueue not found: " + String(name));
    }
    return make_success(info.value());
}

void InquireManager::register_tsqueue(const TSQueueInfo& info) {
    tsqueues_.register_resource(info.name, info);
}

Result<TaskInfo> InquireManager::inquire_task(UInt32 task_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return make_error<TaskInfo>(ErrorCode::NOTFND,
            "Task not found: " + std::to_string(task_id));
    }
    return make_success(it->second);
}

std::vector<TaskInfo> InquireManager::inquire_all_tasks() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TaskInfo> result;
    result.reserve(tasks_.size());
    for (const auto& [id, info] : tasks_) {
        result.push_back(info);
    }
    return result;
}

void InquireManager::register_task(const TaskInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_[info.task_id] = info;
    ++system_info_.current_tasks;
}

void InquireManager::unregister_task(UInt32 task_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tasks_.erase(task_id) > 0) {
        --system_info_.current_tasks;
        ++system_info_.transactions_completed;
    }
}

SystemInfo InquireManager::inquire_system() {
    std::lock_guard<std::mutex> lock(mutex_);
    system_info_.current_tasks = static_cast<UInt32>(tasks_.size());
    return system_info_;
}

void InquireManager::set_system_info(const SystemInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    system_info_ = info;
}

Result<void> InquireManager::set_program_status(StringView name, ResourceStatus status) {
    auto info = programs_.get(name);
    if (!info.has_value()) {
        return make_error<void>(ErrorCode::CICS_PROGRAM_NOT_FOUND,
            "Program not found: " + String(name));
    }
    auto updated = info.value();
    updated.status = status;
    programs_.register_resource(name, updated);
    return make_success();
}

Result<void> InquireManager::set_file_status(StringView name, ResourceStatus status) {
    auto info = files_.get(name);
    if (!info.has_value()) {
        return make_error<void>(ErrorCode::CICS_FILE_NOT_FOUND,
            "File not found: " + String(name));
    }
    auto updated = info.value();
    updated.status = status;
    files_.register_resource(name, updated);
    return make_success();
}

Result<void> InquireManager::set_transaction_status(StringView name, ResourceStatus status) {
    auto info = transactions_.get(name);
    if (!info.has_value()) {
        return make_error<void>(ErrorCode::CICS_TRANSACTION_NOT_FOUND,
            "Transaction not found: " + String(name));
    }
    auto updated = info.value();
    updated.status = status;
    transactions_.register_resource(name, updated);
    return make_success();
}

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

Result<ProgramInfo> exec_cics_inquire_program(StringView name) {
    return InquireManager::instance().inquire_program(name);
}

Result<FileInfo> exec_cics_inquire_file(StringView name) {
    return InquireManager::instance().inquire_file(name);
}

Result<TransactionInfo> exec_cics_inquire_transaction(StringView name) {
    return InquireManager::instance().inquire_transaction(name);
}

Result<TerminalInfo> exec_cics_inquire_terminal(StringView id) {
    return InquireManager::instance().inquire_terminal(id);
}

Result<TDQueueInfo> exec_cics_inquire_tdqueue(StringView name) {
    return InquireManager::instance().inquire_tdqueue(name);
}

Result<TSQueueInfo> exec_cics_inquire_tsqueue(StringView name) {
    return InquireManager::instance().inquire_tsqueue(name);
}

Result<TaskInfo> exec_cics_inquire_task(UInt32 task_id) {
    return InquireManager::instance().inquire_task(task_id);
}

SystemInfo exec_cics_inquire_system() {
    return InquireManager::instance().inquire_system();
}

Result<void> exec_cics_set_program(StringView name, ResourceStatus status) {
    return InquireManager::instance().set_program_status(name, status);
}

Result<void> exec_cics_set_file(StringView name, ResourceStatus status) {
    return InquireManager::instance().set_file_status(name, status);
}

} // namespace inquire
} // namespace cics
