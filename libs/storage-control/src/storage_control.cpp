// =============================================================================
// CICS Emulation - Storage Control Module Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/storage/storage_control.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace cics {
namespace storage {

// =============================================================================
// StoragePool Implementation
// =============================================================================

void StoragePool::record_allocation(UInt32 size) {
    std::lock_guard<std::mutex> lock(mutex_);
    total_size_ += size;
    used_size_ += size;
    ++allocation_count_;
    peak_size_ = std::max(peak_size_, used_size_);
}

void StoragePool::record_free(UInt32 size) {
    std::lock_guard<std::mutex> lock(mutex_);
    used_size_ -= std::min(used_size_, static_cast<UInt64>(size));
}

// =============================================================================
// StorageControlManager Implementation
// =============================================================================

StorageControlManager::StorageControlManager() {
    // Initialize default pools
    pools_[StorageClass::USER] = std::make_unique<StoragePool>(StorageClass::USER);
    pools_[StorageClass::CICSDSA] = std::make_unique<StoragePool>(StorageClass::CICSDSA);
    pools_[StorageClass::CDSA] = std::make_unique<StoragePool>(StorageClass::CDSA);
    pools_[StorageClass::UDSA] = std::make_unique<StoragePool>(StorageClass::UDSA);
    pools_[StorageClass::SDSA] = std::make_unique<StoragePool>(StorageClass::SDSA);
    pools_[StorageClass::RDSA] = std::make_unique<StoragePool>(StorageClass::RDSA);
    pools_[StorageClass::SHARED] = std::make_unique<StoragePool>(StorageClass::SHARED);
}

StorageControlManager::~StorageControlManager() {
    // Free all remaining allocations
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [addr, block] : allocations_) {
        if (block.address) {
            std::free(block.address);
        }
    }
    allocations_.clear();
}

StorageControlManager& StorageControlManager::instance() {
    static StorageControlManager instance;
    return instance;
}

StoragePool& StorageControlManager::get_pool(StorageClass cls) {
    auto it = pools_.find(cls);
    if (it == pools_.end()) {
        pools_[cls] = std::make_unique<StoragePool>(cls);
        return *pools_[cls];
    }
    return *it->second;
}

void StorageControlManager::update_peak() {
    stats_.peak_allocated = std::max(stats_.peak_allocated, stats_.current_allocated);
}

Result<void*> StorageControlManager::getmain(UInt32 size) {
    return getmain(size, StorageClass::USER);
}

Result<void*> StorageControlManager::getmain(UInt32 size, StorageClass cls) {
    return getmain(size, cls, StorageInit::DEFAULT);
}

Result<void*> StorageControlManager::getmain(UInt32 size, StorageClass cls, StorageInit init) {
    return getmain(size, cls, init, false, "");
}

Result<void*> StorageControlManager::getmain(UInt32 size, StorageClass cls, StorageInit init,
                                              bool shared, StringView tag) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.getmain_count;
    
    if (size == 0) {
        return make_error<void*>(ErrorCode::INVALID_ARGUMENT, "Size cannot be zero");
    }
    
    // Check available storage
    if (stats_.current_allocated + size > max_storage_) {
        ++stats_.failed_allocations;
        return make_error<void*>(ErrorCode::OUT_OF_MEMORY,
            "Insufficient storage available");
    }
    
    // Align size
    UInt32 aligned_size = (size + default_alignment_ - 1) & ~(default_alignment_ - 1);
    
    // Allocate memory
    void* address = std::aligned_alloc(default_alignment_, aligned_size);
    if (!address) {
        ++stats_.failed_allocations;
        return make_error<void*>(ErrorCode::OUT_OF_MEMORY,
            "Memory allocation failed");
    }
    
    // Initialize if requested
    switch (init) {
        case StorageInit::ZERO:
        case StorageInit::LOW:
            std::memset(address, 0x00, aligned_size);
            break;
        case StorageInit::HIGH:
            std::memset(address, 0xFF, aligned_size);
            break;
        default:
            break;
    }
    
    // Create block info
    StorageBlock block;
    block.address = address;
    block.size = aligned_size;
    block.requested_size = size;
    block.storage_class = cls;
    block.task_id = 0;  // Would get from task control
    block.shared = shared;
    block.allocation_time = std::chrono::steady_clock::now();
    block.tag = String(tag);
    
    allocations_[address] = block;
    
    // Update statistics
    stats_.total_allocated += aligned_size;
    stats_.current_allocated += aligned_size;
    update_peak();
    
    // Update pool
    get_pool(cls).record_allocation(aligned_size);
    
    return make_success(address);
}

Result<void> StorageControlManager::freemain(void* address) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.freemain_count;
    
    if (!address) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Null address");
    }
    
    auto it = allocations_.find(address);
    if (it == allocations_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Address not found in allocations");
    }
    
    UInt32 size = it->second.size;
    StorageClass cls = it->second.storage_class;
    
    std::free(address);
    allocations_.erase(it);
    
    // Update statistics
    stats_.total_freed += size;
    stats_.current_allocated -= size;
    
    // Update pool
    get_pool(cls).record_free(size);
    
    return make_success();
}

Result<void> StorageControlManager::freemain(void* address, UInt32 size) {
    // Size parameter is for verification only in CICS
    auto result = get_block_info(address);
    if (!result.is_success()) {
        return make_error<void>(result.error().code, result.error().message);
    }
    
    if (result.value().requested_size != size) {
        // Warning: size mismatch, but proceed with free
    }
    
    return freemain(address);
}

Result<void> StorageControlManager::freemain_task(UInt32 task_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<void*> to_free;
    for (const auto& [addr, block] : allocations_) {
        if (block.task_id == task_id && !block.shared) {
            to_free.push_back(addr);
        }
    }
    
    for (void* addr : to_free) {
        auto it = allocations_.find(addr);
        if (it != allocations_.end()) {
            UInt32 size = it->second.size;
            StorageClass cls = it->second.storage_class;
            
            std::free(addr);
            
            stats_.total_freed += size;
            stats_.current_allocated -= size;
            get_pool(cls).record_free(size);
            
            allocations_.erase(it);
        }
    }
    
    return make_success();
}

Result<StorageBlock> StorageControlManager::get_block_info(void* address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = allocations_.find(address);
    if (it == allocations_.end()) {
        return make_error<StorageBlock>(ErrorCode::RECORD_NOT_FOUND,
            "Address not found");
    }
    
    return make_success(it->second);
}

bool StorageControlManager::is_valid_address(void* address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return allocations_.find(address) != allocations_.end();
}

UInt32 StorageControlManager::get_block_size(void* address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = allocations_.find(address);
    return it != allocations_.end() ? it->second.size : 0;
}

UInt64 StorageControlManager::available_storage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return max_storage_ - stats_.current_allocated;
}

UInt64 StorageControlManager::current_allocated() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_.current_allocated;
}

const StoragePool* StorageControlManager::get_pool(StorageClass cls) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pools_.find(cls);
    return it != pools_.end() ? it->second.get() : nullptr;
}

void StorageControlManager::set_max_storage(UInt64 max_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_storage_ = max_bytes;
}

void StorageControlManager::set_default_alignment(UInt32 alignment) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_alignment_ = alignment;
}

String StorageControlManager::get_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "Storage Control Statistics:\n"
        << "  GETMAIN calls:     " << stats_.getmain_count << "\n"
        << "  FREEMAIN calls:    " << stats_.freemain_count << "\n"
        << "  Total allocated:   " << stats_.total_allocated << " bytes\n"
        << "  Total freed:       " << stats_.total_freed << " bytes\n"
        << "  Current allocated: " << stats_.current_allocated << " bytes\n"
        << "  Peak allocated:    " << stats_.peak_allocated << " bytes\n"
        << "  Failed allocs:     " << stats_.failed_allocations << "\n"
        << "  Active blocks:     " << allocations_.size() << "\n"
        << "  Max storage:       " << max_storage_ << " bytes\n"
        << "  Available:         " << (max_storage_ - stats_.current_allocated) << " bytes\n";
    
    return oss.str();
}

void StorageControlManager::reset_statistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Statistics{};
    stats_.current_allocated = 0;
    for (const auto& [addr, block] : allocations_) {
        stats_.current_allocated += block.size;
    }
}

String StorageControlManager::dump_allocations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "Storage Allocations:\n";
    oss << std::setfill('-') << std::setw(80) << "" << "\n" << std::setfill(' ');
    oss << std::left << std::setw(18) << "Address"
        << std::setw(10) << "Size"
        << std::setw(10) << "Requested"
        << std::setw(10) << "Class"
        << std::setw(8) << "Task"
        << "Tag\n";
    oss << std::setfill('-') << std::setw(80) << "" << "\n" << std::setfill(' ');
    
    for (const auto& [addr, block] : allocations_) {
        oss << std::left << std::setw(18) << addr
            << std::setw(10) << block.size
            << std::setw(10) << block.requested_size
            << std::setw(10) << static_cast<int>(block.storage_class)
            << std::setw(8) << block.task_id
            << block.tag << "\n";
    }
    
    return oss.str();
}

// =============================================================================
// EXEC CICS Interface Implementation
// =============================================================================

Result<void*> exec_cics_getmain(UInt32 length) {
    return StorageControlManager::instance().getmain(length);
}

Result<void*> exec_cics_getmain_set(UInt32 length) {
    return StorageControlManager::instance().getmain(length, StorageClass::USER,
        StorageInit::ZERO);
}

Result<void*> exec_cics_getmain_initimg(UInt32 length, Byte init_value) {
    auto result = StorageControlManager::instance().getmain(length);
    if (result.is_success() && result.value()) {
        std::memset(result.value(), init_value, length);
    }
    return result;
}

Result<void*> exec_cics_getmain_shared(UInt32 length) {
    return StorageControlManager::instance().getmain(length, StorageClass::SHARED,
        StorageInit::DEFAULT, true);
}

Result<void> exec_cics_freemain(void* data) {
    return StorageControlManager::instance().freemain(data);
}

Result<void> exec_cics_freemain(void* data, UInt32 length) {
    return StorageControlManager::instance().freemain(data, length);
}

// =============================================================================
// StorageGuard Implementation
// =============================================================================

StorageGuard::StorageGuard(UInt32 size) : size_(size) {
    auto result = StorageControlManager::instance().getmain(size);
    if (result.is_success()) {
        address_ = result.value();
    }
}

StorageGuard::StorageGuard(UInt32 size, StorageClass cls) : size_(size) {
    auto result = StorageControlManager::instance().getmain(size, cls);
    if (result.is_success()) {
        address_ = result.value();
    }
}

StorageGuard::~StorageGuard() {
    if (address_) {
        StorageControlManager::instance().freemain(address_);
    }
}

StorageGuard::StorageGuard(StorageGuard&& other) noexcept
    : address_(other.address_), size_(other.size_) {
    other.address_ = nullptr;
    other.size_ = 0;
}

StorageGuard& StorageGuard::operator=(StorageGuard&& other) noexcept {
    if (this != &other) {
        if (address_) {
            StorageControlManager::instance().freemain(address_);
        }
        address_ = other.address_;
        size_ = other.size_;
        other.address_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

void* StorageGuard::release() {
    void* addr = address_;
    address_ = nullptr;
    size_ = 0;
    return addr;
}

void StorageGuard::reset() {
    if (address_) {
        StorageControlManager::instance().freemain(address_);
        address_ = nullptr;
        size_ = 0;
    }
}

// =============================================================================
// Utility Functions Implementation
// =============================================================================

void storage_init_zero(void* address, UInt32 size) {
    if (address && size > 0) {
        std::memset(address, 0, size);
    }
}

void storage_init_value(void* address, UInt32 size, Byte value) {
    if (address && size > 0) {
        std::memset(address, value, size);
    }
}

void storage_copy(void* dest, const void* src, UInt32 size) {
    if (dest && src && size > 0) {
        std::memcpy(dest, src, size);
    }
}

void storage_move(void* dest, const void* src, UInt32 size) {
    if (dest && src && size > 0) {
        std::memmove(dest, src, size);
    }
}

Int32 storage_compare(const void* a, const void* b, UInt32 size) {
    if (!a || !b || size == 0) return 0;
    return std::memcmp(a, b, size);
}

bool storage_equal(const void* a, const void* b, UInt32 size) {
    return storage_compare(a, b, size) == 0;
}

} // namespace storage
} // namespace cics
