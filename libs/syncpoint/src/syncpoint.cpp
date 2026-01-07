// =============================================================================
// CICS Emulation - Syncpoint Control Implementation
// =============================================================================

#include <cics/syncpoint/syncpoint.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace cics {
namespace syncpoint {

// Thread-local current UOW ID
thread_local String SyncpointManager::current_uow_id_;

// =============================================================================
// Utility Functions
// =============================================================================

String uow_state_to_string(UOWState state) {
    switch (state) {
        case UOWState::ACTIVE: return "ACTIVE";
        case UOWState::COMMITTED: return "COMMITTED";
        case UOWState::ROLLED_BACK: return "ROLLED_BACK";
        case UOWState::IN_DOUBT: return "IN_DOUBT";
        case UOWState::SHUNTED: return "SHUNTED";
        default: return "UNKNOWN";
    }
}

String resource_type_to_string(ResourceType type) {
    switch (type) {
        case ResourceType::VSAM_FILE: return "VSAM_FILE";
        case ResourceType::TSQ: return "TSQ";
        case ResourceType::TDQ: return "TDQ";
        case ResourceType::ENQUEUE: return "ENQUEUE";
        case ResourceType::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// SimpleRecoveryResource Implementation
// =============================================================================

SimpleRecoveryResource::SimpleRecoveryResource(StringView name, ResourceType type,
                                               PrepareCallback prepare_cb,
                                               CommitCallback commit_cb,
                                               RollbackCallback rollback_cb)
    : name_(name), type_(type)
    , prepare_cb_(std::move(prepare_cb))
    , commit_cb_(std::move(commit_cb))
    , rollback_cb_(std::move(rollback_cb))
{
}

Result<void> SimpleRecoveryResource::prepare() {
    if (prepare_cb_) {
        auto result = prepare_cb_();
        if (result.is_success()) {
            prepared_ = true;
        }
        return result;
    }
    prepared_ = true;
    return make_success();
}

Result<void> SimpleRecoveryResource::commit() {
    if (commit_cb_) {
        return commit_cb_();
    }
    return make_success();
}

Result<void> SimpleRecoveryResource::rollback() {
    prepared_ = false;
    if (rollback_cb_) {
        return rollback_cb_();
    }
    return make_success();
}

// =============================================================================
// UnitOfWork Implementation
// =============================================================================

UnitOfWork::UnitOfWork(StringView id)
    : id_(id)
    , start_time_(std::chrono::steady_clock::now())
{
}

UnitOfWork::~UnitOfWork() {
    if (state_ == UOWState::ACTIVE) {
        // Auto-rollback uncommitted UOW
        rollback();
    }
}

Result<void> UnitOfWork::register_resource(std::shared_ptr<RecoveryResource> resource) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != UOWState::ACTIVE) {
        return make_error<void>(ErrorCode::INVALID_STATE,
            "Cannot register resource: UOW is not active");
    }
    
    // Check for duplicate
    for (const auto& r : resources_) {
        if (r->name() == resource->name()) {
            return make_error<void>(ErrorCode::DUPLICATE_KEY,
                "Resource already registered: " + resource->name());
        }
    }
    
    resources_.push_back(std::move(resource));
    return make_success();
}

Result<void> UnitOfWork::unregister_resource(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(resources_.begin(), resources_.end(),
        [&name](const auto& r) { return r->name() == name; });
    
    if (it == resources_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Resource not found: " + String(name));
    }
    
    resources_.erase(it);
    return make_success();
}

bool UnitOfWork::has_resource(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::any_of(resources_.begin(), resources_.end(),
        [&name](const auto& r) { return r->name() == name; });
}

UInt32 UnitOfWork::resource_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<UInt32>(resources_.size());
}

Result<void> UnitOfWork::syncpoint() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != UOWState::ACTIVE) {
        return make_error<void>(ErrorCode::INVALID_STATE,
            "Cannot syncpoint: UOW is not active");
    }
    
    // Two-phase commit: prepare then commit
    auto prepare_result = prepare_all();
    if (prepare_result.is_error()) {
        // Prepare failed, rollback
        rollback_all();
        state_ = UOWState::ROLLED_BACK;
        end_time_ = std::chrono::steady_clock::now();
        return prepare_result;
    }
    
    auto commit_result = commit_all();
    if (commit_result.is_error()) {
        // Commit failed after prepare - this is an in-doubt situation
        state_ = UOWState::IN_DOUBT;
        end_time_ = std::chrono::steady_clock::now();
        return commit_result;
    }
    
    ++syncpoint_count_;
    state_ = UOWState::COMMITTED;
    end_time_ = std::chrono::steady_clock::now();
    return make_success();
}

Result<void> UnitOfWork::rollback() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != UOWState::ACTIVE && state_ != UOWState::IN_DOUBT) {
        return make_error<void>(ErrorCode::INVALID_STATE,
            "Cannot rollback: UOW is not in rollbackable state");
    }
    
    auto result = rollback_all();
    ++rollback_count_;
    state_ = UOWState::ROLLED_BACK;
    end_time_ = std::chrono::steady_clock::now();
    return result;
}

Result<void> UnitOfWork::prepare_all() {
    for (auto& resource : resources_) {
        auto result = resource->prepare();
        if (result.is_error()) {
            return result;
        }
    }
    return make_success();
}

Result<void> UnitOfWork::commit_all() {
    for (auto& resource : resources_) {
        auto result = resource->commit();
        if (result.is_error()) {
            return result;
        }
    }
    return make_success();
}

Result<void> UnitOfWork::rollback_all() {
    Result<void> last_error = make_success();
    
    // Rollback in reverse order
    for (auto it = resources_.rbegin(); it != resources_.rend(); ++it) {
        auto result = (*it)->rollback();
        if (result.is_error()) {
            last_error = result;  // Continue rolling back other resources
        }
    }
    
    return last_error;
}

UOWInfo UnitOfWork::get_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    UOWInfo info;
    info.uow_id = id_;
    info.state = state_;
    info.start_time = start_time_;
    info.end_time = end_time_;
    info.resource_count = static_cast<UInt32>(resources_.size());
    info.syncpoint_count = syncpoint_count_;
    info.rollback_count = rollback_count_;
    return info;
}

std::vector<String> UnitOfWork::list_resources() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<String> names;
    names.reserve(resources_.size());
    for (const auto& r : resources_) {
        names.push_back(r->name());
    }
    return names;
}

// =============================================================================
// SyncpointManager Implementation
// =============================================================================

SyncpointManager& SyncpointManager::instance() {
    static SyncpointManager instance;
    return instance;
}

void SyncpointManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;
    
    uows_.clear();
    uow_counter_ = 0;
    reset_stats();
    initialized_ = true;
}

void SyncpointManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Rollback any active UOWs
    for (auto& [id, uow] : uows_) {
        if (uow->is_active()) {
            uow->rollback();
        }
    }
    
    uows_.clear();
    current_uow_id_.clear();
    initialized_ = false;
}

String SyncpointManager::generate_uow_id() {
    UInt64 counter = ++uow_counter_;
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << "UOW" << std::put_time(std::localtime(&time_t_now), "%Y%m%d%H%M%S")
        << std::setfill('0') << std::setw(8) << counter;
    return oss.str();
}

Result<String> SyncpointManager::begin_uow() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return make_error<String>(ErrorCode::NOT_INITIALIZED,
            "SyncpointManager not initialized");
    }
    
    String id = generate_uow_id();
    auto uow = std::make_unique<UnitOfWork>(id);
    uows_[id] = std::move(uow);
    current_uow_id_ = id;
    
    ++stats_.uows_created;
    return make_success(id);
}

Result<void> SyncpointManager::end_uow(StringView uow_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = uows_.find(String(uow_id));
    if (it == uows_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "UOW not found: " + String(uow_id));
    }
    
    // Remove the UOW
    uows_.erase(it);
    
    if (current_uow_id_ == uow_id) {
        current_uow_id_.clear();
    }
    
    return make_success();
}

UnitOfWork* SyncpointManager::current_uow() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (current_uow_id_.empty()) {
        return nullptr;
    }
    
    auto it = uows_.find(current_uow_id_);
    if (it == uows_.end()) {
        return nullptr;
    }
    
    return it->second.get();
}

UnitOfWork* SyncpointManager::get_uow(StringView uow_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = uows_.find(String(uow_id));
    if (it == uows_.end()) {
        return nullptr;
    }
    
    return it->second.get();
}

Result<void> SyncpointManager::syncpoint() {
    auto* uow = current_uow();
    if (!uow) {
        // No active UOW, create one implicitly
        auto result = begin_uow();
        if (result.is_error()) {
            return make_error<void>(result.error().code, result.error().message);
        }
        uow = current_uow();
    }
    
    auto result = uow->syncpoint();
    if (result.is_success()) {
        ++stats_.syncpoints_issued;
        ++stats_.uows_committed;
    } else {
        ++stats_.commit_failures;
    }
    
    return result;
}

Result<void> SyncpointManager::syncpoint(StringView uow_id) {
    auto* uow = get_uow(uow_id);
    if (!uow) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "UOW not found: " + String(uow_id));
    }
    
    auto result = uow->syncpoint();
    if (result.is_success()) {
        ++stats_.syncpoints_issued;
        ++stats_.uows_committed;
    } else {
        ++stats_.commit_failures;
    }
    
    return result;
}

Result<void> SyncpointManager::rollback() {
    auto* uow = current_uow();
    if (!uow) {
        return make_error<void>(ErrorCode::INVALID_STATE,
            "No active UOW to rollback");
    }
    
    auto result = uow->rollback();
    if (result.is_success()) {
        ++stats_.rollbacks_issued;
        ++stats_.uows_rolled_back;
    } else {
        ++stats_.rollback_failures;
    }
    
    return result;
}

Result<void> SyncpointManager::rollback(StringView uow_id) {
    auto* uow = get_uow(uow_id);
    if (!uow) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "UOW not found: " + String(uow_id));
    }
    
    auto result = uow->rollback();
    if (result.is_success()) {
        ++stats_.rollbacks_issued;
        ++stats_.uows_rolled_back;
    } else {
        ++stats_.rollback_failures;
    }
    
    return result;
}

Result<void> SyncpointManager::register_resource(std::shared_ptr<RecoveryResource> resource) {
    auto* uow = current_uow();
    if (!uow) {
        // Auto-create UOW
        auto result = begin_uow();
        if (result.is_error()) {
            return make_error<void>(result.error().code, result.error().message);
        }
        uow = current_uow();
    }
    
    auto result = uow->register_resource(std::move(resource));
    if (result.is_success()) {
        ++stats_.resources_registered;
    }
    return result;
}

Result<void> SyncpointManager::register_resource(StringView name, ResourceType type,
                                                  PrepareCallback prepare,
                                                  CommitCallback commit,
                                                  RollbackCallback rollback) {
    auto resource = std::make_shared<SimpleRecoveryResource>(
        name, type, std::move(prepare), std::move(commit), std::move(rollback));
    return register_resource(std::move(resource));
}

SyncpointStats SyncpointManager::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

std::vector<UOWInfo> SyncpointManager::list_uows() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<UOWInfo> list;
    list.reserve(uows_.size());
    for (const auto& [id, uow] : uows_) {
        list.push_back(uow->get_info());
    }
    return list;
}

void SyncpointManager::reset_stats() {
    stats_.syncpoints_issued = 0;
    stats_.rollbacks_issued = 0;
    stats_.uows_created = 0;
    stats_.uows_committed = 0;
    stats_.uows_rolled_back = 0;
    stats_.resources_registered = 0;
    stats_.prepare_failures = 0;
    stats_.commit_failures = 0;
    stats_.rollback_failures = 0;
}

// =============================================================================
// SyncpointGuard Implementation
// =============================================================================

SyncpointGuard::SyncpointGuard(bool auto_commit)
    : auto_commit_(auto_commit)
{
    auto result = SyncpointManager::instance().begin_uow();
    if (result.is_success()) {
        uow_id_ = result.value();
        active_ = true;
    }
}

SyncpointGuard::~SyncpointGuard() {
    if (active_) {
        if (auto_commit_) {
            commit();
        } else {
            rollback();
        }
    }
}

SyncpointGuard::SyncpointGuard(SyncpointGuard&& other) noexcept
    : uow_id_(std::move(other.uow_id_))
    , auto_commit_(other.auto_commit_)
    , active_(other.active_)
{
    other.active_ = false;
}

SyncpointGuard& SyncpointGuard::operator=(SyncpointGuard&& other) noexcept {
    if (this != &other) {
        if (active_) {
            rollback();
        }
        uow_id_ = std::move(other.uow_id_);
        auto_commit_ = other.auto_commit_;
        active_ = other.active_;
        other.active_ = false;
    }
    return *this;
}

Result<void> SyncpointGuard::commit() {
    if (!active_) {
        return make_error<void>(ErrorCode::INVALID_STATE,
            "SyncpointGuard is not active");
    }
    
    auto result = SyncpointManager::instance().syncpoint(uow_id_);
    active_ = false;
    return result;
}

Result<void> SyncpointGuard::rollback() {
    if (!active_) {
        return make_error<void>(ErrorCode::INVALID_STATE,
            "SyncpointGuard is not active");
    }
    
    auto result = SyncpointManager::instance().rollback(uow_id_);
    active_ = false;
    return result;
}

void SyncpointGuard::release() {
    active_ = false;
}

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

Result<void> exec_cics_syncpoint() {
    return SyncpointManager::instance().syncpoint();
}

Result<void> exec_cics_syncpoint_rollback() {
    return SyncpointManager::instance().rollback();
}

Result<void> exec_cics_syncpoint_rollbackuow() {
    return SyncpointManager::instance().rollback();
}

} // namespace syncpoint
} // namespace cics
