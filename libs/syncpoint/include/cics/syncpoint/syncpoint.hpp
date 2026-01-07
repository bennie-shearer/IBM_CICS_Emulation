// =============================================================================
// CICS Emulation - Syncpoint Control
// =============================================================================
// Provides SYNCPOINT and ROLLBACK functionality for transaction management.
// Implements unit of work (UOW) concepts for maintaining data consistency.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#ifndef CICS_SYNCPOINT_HPP
#define CICS_SYNCPOINT_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <functional>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <atomic>

namespace cics {
namespace syncpoint {

// =============================================================================
// Forward Declarations
// =============================================================================

class SyncpointManager;
class UnitOfWork;
class RecoveryResource;

// =============================================================================
// Enumerations
// =============================================================================

enum class UOWState {
    ACTIVE,         // UOW is active
    COMMITTED,      // UOW has been committed
    ROLLED_BACK,    // UOW has been rolled back
    IN_DOUBT,       // UOW outcome is uncertain
    SHUNTED         // UOW is shunted (waiting for resolution)
};

enum class ResourceType {
    VSAM_FILE,      // VSAM file resource
    TSQ,            // Temporary Storage Queue
    TDQ,            // Transient Data Queue
    ENQUEUE,        // ENQ resource
    CUSTOM          // Custom user resource
};

enum class SyncpointOption {
    NONE,           // No special options
    ROLLBACK_ONLY   // Only rollback is allowed
};

// =============================================================================
// Recovery Resource Interface
// =============================================================================

class RecoveryResource {
public:
    virtual ~RecoveryResource() = default;
    
    virtual ResourceType type() const = 0;
    virtual String name() const = 0;
    virtual Result<void> prepare() = 0;
    virtual Result<void> commit() = 0;
    virtual Result<void> rollback() = 0;
    virtual bool is_prepared() const = 0;
};

// =============================================================================
// Recovery Callback Types
// =============================================================================

using PrepareCallback = std::function<Result<void>()>;
using CommitCallback = std::function<Result<void>()>;
using RollbackCallback = std::function<Result<void>()>;

// =============================================================================
// Simple Recovery Resource
// =============================================================================

class SimpleRecoveryResource : public RecoveryResource {
public:
    SimpleRecoveryResource(StringView name, ResourceType type,
                           PrepareCallback prepare_cb,
                           CommitCallback commit_cb,
                           RollbackCallback rollback_cb);
    
    ResourceType type() const override { return type_; }
    String name() const override { return name_; }
    Result<void> prepare() override;
    Result<void> commit() override;
    Result<void> rollback() override;
    bool is_prepared() const override { return prepared_; }
    
private:
    String name_;
    ResourceType type_;
    PrepareCallback prepare_cb_;
    CommitCallback commit_cb_;
    RollbackCallback rollback_cb_;
    bool prepared_ = false;
};

// =============================================================================
// Unit of Work
// =============================================================================

struct UOWInfo {
    String uow_id;
    UOWState state;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    UInt32 resource_count;
    UInt32 syncpoint_count;
    UInt32 rollback_count;
};

class UnitOfWork {
public:
    explicit UnitOfWork(StringView id);
    ~UnitOfWork();
    
    // Properties
    [[nodiscard]] const String& id() const { return id_; }
    [[nodiscard]] UOWState state() const { return state_; }
    [[nodiscard]] bool is_active() const { return state_ == UOWState::ACTIVE; }
    
    // Resource management
    Result<void> register_resource(std::shared_ptr<RecoveryResource> resource);
    Result<void> unregister_resource(StringView name);
    [[nodiscard]] bool has_resource(StringView name) const;
    [[nodiscard]] UInt32 resource_count() const;
    
    // Syncpoint operations
    Result<void> syncpoint();
    Result<void> rollback();
    
    // Information
    [[nodiscard]] UOWInfo get_info() const;
    [[nodiscard]] std::vector<String> list_resources() const;
    
private:
    Result<void> prepare_all();
    Result<void> commit_all();
    Result<void> rollback_all();
    
    String id_;
    UOWState state_ = UOWState::ACTIVE;
    std::vector<std::shared_ptr<RecoveryResource>> resources_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
    UInt32 syncpoint_count_ = 0;
    UInt32 rollback_count_ = 0;
    mutable std::mutex mutex_;
};

// =============================================================================
// Syncpoint Statistics
// =============================================================================

struct SyncpointStats {
    UInt64 syncpoints_issued{0};
    UInt64 rollbacks_issued{0};
    UInt64 uows_created{0};
    UInt64 uows_committed{0};
    UInt64 uows_rolled_back{0};
    UInt64 resources_registered{0};
    UInt64 prepare_failures{0};
    UInt64 commit_failures{0};
    UInt64 rollback_failures{0};
};

// =============================================================================
// Syncpoint Manager
// =============================================================================

class SyncpointManager {
public:
    static SyncpointManager& instance();
    
    // Lifecycle
    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }
    
    // UOW management
    Result<String> begin_uow();
    Result<void> end_uow(StringView uow_id);
    [[nodiscard]] UnitOfWork* current_uow();
    [[nodiscard]] UnitOfWork* get_uow(StringView uow_id);
    
    // Syncpoint operations
    Result<void> syncpoint();
    Result<void> syncpoint(StringView uow_id);
    Result<void> rollback();
    Result<void> rollback(StringView uow_id);
    
    // Resource registration (for current UOW)
    Result<void> register_resource(std::shared_ptr<RecoveryResource> resource);
    Result<void> register_resource(StringView name, ResourceType type,
                                    PrepareCallback prepare,
                                    CommitCallback commit,
                                    RollbackCallback rollback);
    
    // Statistics and info
    [[nodiscard]] SyncpointStats get_stats() const;
    [[nodiscard]] std::vector<UOWInfo> list_uows() const;
    void reset_stats();
    
private:
    SyncpointManager() = default;
    ~SyncpointManager() = default;
    SyncpointManager(const SyncpointManager&) = delete;
    SyncpointManager& operator=(const SyncpointManager&) = delete;
    
    String generate_uow_id();
    
    bool initialized_ = false;
    std::unordered_map<String, std::unique_ptr<UnitOfWork>> uows_;
    thread_local static String current_uow_id_;
    SyncpointStats stats_;
    std::atomic<UInt64> uow_counter_{0};
    mutable std::mutex mutex_;
};

// =============================================================================
// RAII Syncpoint Guard
// =============================================================================

class SyncpointGuard {
public:
    explicit SyncpointGuard(bool auto_commit = true);
    ~SyncpointGuard();
    
    // Disable copy
    SyncpointGuard(const SyncpointGuard&) = delete;
    SyncpointGuard& operator=(const SyncpointGuard&) = delete;
    
    // Move support
    SyncpointGuard(SyncpointGuard&& other) noexcept;
    SyncpointGuard& operator=(SyncpointGuard&& other) noexcept;
    
    [[nodiscard]] bool is_active() const { return active_; }
    [[nodiscard]] const String& uow_id() const { return uow_id_; }
    
    Result<void> commit();
    Result<void> rollback();
    void release();
    
private:
    String uow_id_;
    bool auto_commit_;
    bool active_ = false;
};

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

Result<void> exec_cics_syncpoint();
Result<void> exec_cics_syncpoint_rollback();
Result<void> exec_cics_syncpoint_rollbackuow();

// =============================================================================
// Utility Functions
// =============================================================================

[[nodiscard]] String uow_state_to_string(UOWState state);
[[nodiscard]] String resource_type_to_string(ResourceType type);

} // namespace syncpoint
} // namespace cics

#endif // CICS_SYNCPOINT_HPP
