// =============================================================================
// CICS Emulation - Inquire Services
// =============================================================================
// Provides INQUIRE PROGRAM, FILE, TRANSACTION, TERMINAL, etc. functionality.
// Implements runtime status queries for CICS resources.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#ifndef CICS_INQUIRE_HPP
#define CICS_INQUIRE_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <functional>

namespace cics {
namespace inquire {

// =============================================================================
// Enumerations
// =============================================================================

enum class ResourceType {
    PROGRAM,
    FILE,
    TRANSACTION,
    TERMINAL,
    TDQUEUE,
    TSQUEUE,
    CONNECTION,
    TASK,
    SYSTEM
};

enum class ResourceStatus {
    ENABLED,
    DISABLED,
    UNENABLED,
    CLOSED,
    OPEN,
    ACTIVE,
    SUSPENDED,
    UNKNOWN
};

enum class EnableStatus {
    ENABLED,
    DISABLED,
    PENDING
};

enum class OpenStatus {
    OPEN,
    CLOSED,
    CLOSING,
    OPENING
};

// =============================================================================
// Resource Information Structures
// =============================================================================

struct ProgramInfo {
    String name;
    ResourceStatus status = ResourceStatus::ENABLED;
    String language;            // COBOL, C, CPP, ASM
    UInt32 length = 0;          // Program size
    UInt32 use_count = 0;       // Current use count
    UInt32 resident_count = 0;  // Times loaded
    bool resident = false;      // Resident in memory
    bool sharable = true;       // Can be shared
    std::chrono::system_clock::time_point installed;
};

struct FileInfo {
    String name;
    ResourceStatus status = ResourceStatus::ENABLED;
    OpenStatus open_status = OpenStatus::CLOSED;
    String type;                // VSAM, BDAM, etc.
    String access_method;       // KSDS, ESDS, RRDS
    bool read_enabled = true;
    bool update_enabled = true;
    bool add_enabled = true;
    bool delete_enabled = true;
    bool browse_enabled = true;
    UInt64 records_read = 0;
    UInt64 records_written = 0;
    String dsname;
};

struct TransactionInfo {
    String name;
    ResourceStatus status = ResourceStatus::ENABLED;
    String program;             // Initial program
    UInt32 priority = 1;
    UInt32 task_data_key = 0;
    bool dynamic = false;
    bool protected_res = false;
    UInt32 active_count = 0;    // Currently active
    UInt64 total_count = 0;     // Total executions
};

struct TerminalInfo {
    String id;
    ResourceStatus status = ResourceStatus::ENABLED;
    String type;                // 3270, VT100, etc.
    UInt16 rows = 24;
    UInt16 columns = 80;
    bool in_service = true;
    String user_id;
    String transaction;
    String network_name;
};

struct TDQueueInfo {
    String name;
    ResourceStatus status = ResourceStatus::ENABLED;
    String type;                // INTRA, EXTRA
    UInt32 depth = 0;
    UInt32 max_depth = 0;
    bool trigger_enabled = false;
    String trigger_transaction;
    UInt32 trigger_level = 1;
};

struct TSQueueInfo {
    String name;
    UInt32 item_count = 0;
    UInt64 total_size = 0;
    bool recoverable = false;
    String location;            // MAIN, AUXILIARY
};

struct TaskInfo {
    UInt32 task_id;
    String transaction;
    String program;
    ResourceStatus status = ResourceStatus::ACTIVE;
    String terminal;
    String user_id;
    std::chrono::steady_clock::time_point start_time;
    UInt32 priority;
};

struct SystemInfo {
    String applid;
    String release;
    String version;
    std::chrono::system_clock::time_point startup_time;
    UInt32 max_tasks = 999;
    UInt32 current_tasks = 0;
    UInt64 transactions_completed = 0;
    String status;
};

// =============================================================================
// Resource Registry
// =============================================================================

template<typename T>
class ResourceRegistry {
public:
    void register_resource(StringView name, const T& info) {
        std::lock_guard<std::mutex> lock(mutex_);
        resources_[String(name)] = info;
    }
    
    void unregister_resource(StringView name) {
        std::lock_guard<std::mutex> lock(mutex_);
        resources_.erase(String(name));
    }
    
    [[nodiscard]] Optional<T> get(StringView name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = resources_.find(String(name));
        if (it != resources_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    [[nodiscard]] bool exists(StringView name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return resources_.find(String(name)) != resources_.end();
    }
    
    [[nodiscard]] std::vector<String> list() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<String> names;
        names.reserve(resources_.size());
        for (const auto& [name, info] : resources_) {
            names.push_back(name);
        }
        return names;
    }
    
    [[nodiscard]] std::vector<T> list_all() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T> items;
        items.reserve(resources_.size());
        for (const auto& [name, info] : resources_) {
            items.push_back(info);
        }
        return items;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        resources_.clear();
    }
    
    [[nodiscard]] UInt32 count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<UInt32>(resources_.size());
    }

private:
    std::unordered_map<String, T> resources_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Inquire Manager
// =============================================================================

class InquireManager {
public:
    static InquireManager& instance();
    
    // Lifecycle
    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }
    
    // Program inquiries
    Result<ProgramInfo> inquire_program(StringView name);
    [[nodiscard]] std::vector<ProgramInfo> inquire_all_programs();
    void register_program(const ProgramInfo& info);
    
    // File inquiries
    Result<FileInfo> inquire_file(StringView name);
    [[nodiscard]] std::vector<FileInfo> inquire_all_files();
    void register_file(const FileInfo& info);
    
    // Transaction inquiries
    Result<TransactionInfo> inquire_transaction(StringView name);
    [[nodiscard]] std::vector<TransactionInfo> inquire_all_transactions();
    void register_transaction(const TransactionInfo& info);
    
    // Terminal inquiries
    Result<TerminalInfo> inquire_terminal(StringView id);
    [[nodiscard]] std::vector<TerminalInfo> inquire_all_terminals();
    void register_terminal(const TerminalInfo& info);
    
    // TDQ inquiries
    Result<TDQueueInfo> inquire_tdqueue(StringView name);
    [[nodiscard]] std::vector<TDQueueInfo> inquire_all_tdqueues();
    void register_tdqueue(const TDQueueInfo& info);
    
    // TSQ inquiries
    Result<TSQueueInfo> inquire_tsqueue(StringView name);
    void register_tsqueue(const TSQueueInfo& info);
    
    // Task inquiries
    Result<TaskInfo> inquire_task(UInt32 task_id);
    [[nodiscard]] std::vector<TaskInfo> inquire_all_tasks();
    void register_task(const TaskInfo& info);
    void unregister_task(UInt32 task_id);
    
    // System inquiry
    [[nodiscard]] SystemInfo inquire_system();
    void set_system_info(const SystemInfo& info);
    
    // SET operations (modify resource status)
    Result<void> set_program_status(StringView name, ResourceStatus status);
    Result<void> set_file_status(StringView name, ResourceStatus status);
    Result<void> set_transaction_status(StringView name, ResourceStatus status);
    
private:
    InquireManager() = default;
    ~InquireManager() = default;
    InquireManager(const InquireManager&) = delete;
    InquireManager& operator=(const InquireManager&) = delete;
    
    bool initialized_ = false;
    
    ResourceRegistry<ProgramInfo> programs_;
    ResourceRegistry<FileInfo> files_;
    ResourceRegistry<TransactionInfo> transactions_;
    ResourceRegistry<TerminalInfo> terminals_;
    ResourceRegistry<TDQueueInfo> tdqueues_;
    ResourceRegistry<TSQueueInfo> tsqueues_;
    std::unordered_map<UInt32, TaskInfo> tasks_;
    SystemInfo system_info_;
    
    mutable std::mutex mutex_;
};

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

// INQUIRE PROGRAM
Result<ProgramInfo> exec_cics_inquire_program(StringView name);

// INQUIRE FILE
Result<FileInfo> exec_cics_inquire_file(StringView name);

// INQUIRE TRANSACTION
Result<TransactionInfo> exec_cics_inquire_transaction(StringView name);

// INQUIRE TERMINAL
Result<TerminalInfo> exec_cics_inquire_terminal(StringView id);

// INQUIRE TDQUEUE
Result<TDQueueInfo> exec_cics_inquire_tdqueue(StringView name);

// INQUIRE TSQUEUE
Result<TSQueueInfo> exec_cics_inquire_tsqueue(StringView name);

// INQUIRE TASK
Result<TaskInfo> exec_cics_inquire_task(UInt32 task_id);

// INQUIRE SYSTEM
SystemInfo exec_cics_inquire_system();

// SET operations
Result<void> exec_cics_set_program(StringView name, ResourceStatus status);
Result<void> exec_cics_set_file(StringView name, ResourceStatus status);

// =============================================================================
// Utility Functions
// =============================================================================

[[nodiscard]] String resource_type_to_string(ResourceType type);
[[nodiscard]] String resource_status_to_string(ResourceStatus status);
[[nodiscard]] String enable_status_to_string(EnableStatus status);
[[nodiscard]] String open_status_to_string(OpenStatus status);

} // namespace inquire
} // namespace cics

#endif // CICS_INQUIRE_HPP
