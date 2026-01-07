// =============================================================================
// CICS Emulation - Storage Control Module
// Version: 3.4.6
// =============================================================================
// Implements CICS Storage Control services:
// - GETMAIN: Acquire storage
// - FREEMAIN: Release storage
// - Storage pools and tracking
// =============================================================================

#ifndef CICS_STORAGE_CONTROL_HPP
#define CICS_STORAGE_CONTROL_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <vector>
#include <cstring>

namespace cics {
namespace storage {

// =============================================================================
// Storage Classes
// =============================================================================

enum class StorageClass : UInt8 {
    USER,       // User storage (default)
    CICSDSA,    // CICS Dynamic Storage Area
    CDSA,       // CICS DSA
    UDSA,       // User DSA
    SDSA,       // Shared DSA
    RDSA,       // Read-only DSA
    SHARED      // Shared storage (cross-transaction)
};

enum class StorageInit : UInt8 {
    DEFAULT,    // No initialization
    ZERO,       // Initialize to zeros
    HIGH,       // Initialize to 0xFF
    LOW         // Initialize to 0x00 (same as ZERO)
};

// =============================================================================
// Storage Block
// =============================================================================

struct StorageBlock {
    void* address = nullptr;
    UInt32 size = 0;
    UInt32 requested_size = 0;
    StorageClass storage_class = StorageClass::USER;
    UInt32 task_id = 0;
    bool shared = false;
    std::chrono::steady_clock::time_point allocation_time;
    String tag;  // Optional identification tag
    
    [[nodiscard]] bool is_valid() const { return address != nullptr && size > 0; }
};

// =============================================================================
// Storage Pool
// =============================================================================

class StoragePool {
private:
    StorageClass class_;
    UInt64 total_size_ = 0;
    UInt64 used_size_ = 0;
    UInt64 peak_size_ = 0;
    UInt32 allocation_count_ = 0;
    mutable std::mutex mutex_;
    
public:
    explicit StoragePool(StorageClass cls) : class_(cls) {}
    
    void record_allocation(UInt32 size);
    void record_free(UInt32 size);
    
    [[nodiscard]] StorageClass storage_class() const { return class_; }
    [[nodiscard]] UInt64 total_allocated() const { return total_size_; }
    [[nodiscard]] UInt64 current_used() const { return used_size_; }
    [[nodiscard]] UInt64 peak_used() const { return peak_size_; }
    [[nodiscard]] UInt32 allocation_count() const { return allocation_count_; }
};

// =============================================================================
// Storage Control Manager
// =============================================================================

class StorageControlManager {
private:
    mutable std::mutex mutex_;
    
    // Storage tracking
    std::unordered_map<void*, StorageBlock> allocations_;
    std::unordered_map<StorageClass, std::unique_ptr<StoragePool>> pools_;
    
    // Configuration
    UInt64 max_storage_ = 64 * 1024 * 1024;  // 64MB default
    UInt32 default_alignment_ = 8;
    
    // Statistics
    struct Statistics {
        UInt64 getmain_count = 0;
        UInt64 freemain_count = 0;
        UInt64 total_allocated = 0;
        UInt64 total_freed = 0;
        UInt64 peak_allocated = 0;
        UInt64 current_allocated = 0;
        UInt64 failed_allocations = 0;
    } stats_;
    
    void update_peak();
    StoragePool& get_pool(StorageClass cls);
    
public:
    StorageControlManager();
    ~StorageControlManager();
    
    // Singleton access
    static StorageControlManager& instance();
    
    // GETMAIN - Acquire storage
    Result<void*> getmain(UInt32 size);
    Result<void*> getmain(UInt32 size, StorageClass cls);
    Result<void*> getmain(UInt32 size, StorageClass cls, StorageInit init);
    Result<void*> getmain(UInt32 size, StorageClass cls, StorageInit init, 
                          bool shared, StringView tag = "");
    
    // FREEMAIN - Release storage
    Result<void> freemain(void* address);
    Result<void> freemain(void* address, UInt32 size);
    Result<void> freemain_task(UInt32 task_id);  // Free all storage for task
    
    // Query operations
    [[nodiscard]] Result<StorageBlock> get_block_info(void* address) const;
    [[nodiscard]] bool is_valid_address(void* address) const;
    [[nodiscard]] UInt32 get_block_size(void* address) const;
    [[nodiscard]] UInt64 available_storage() const;
    [[nodiscard]] UInt64 current_allocated() const;
    
    // Pool management
    [[nodiscard]] const StoragePool* get_pool(StorageClass cls) const;
    
    // Configuration
    void set_max_storage(UInt64 max_bytes);
    void set_default_alignment(UInt32 alignment);
    
    // Statistics
    [[nodiscard]] String get_statistics() const;
    void reset_statistics();
    
    // Storage dump
    [[nodiscard]] String dump_allocations() const;
};

// =============================================================================
// EXEC CICS Style Interface
// =============================================================================

Result<void*> exec_cics_getmain(UInt32 length);
Result<void*> exec_cics_getmain_set(UInt32 length);
Result<void*> exec_cics_getmain_initimg(UInt32 length, Byte init_value);
Result<void*> exec_cics_getmain_shared(UInt32 length);
Result<void> exec_cics_freemain(void* data);
Result<void> exec_cics_freemain(void* data, UInt32 length);

// =============================================================================
// RAII Storage Guard
// =============================================================================

class StorageGuard {
private:
    void* address_ = nullptr;
    UInt32 size_ = 0;
    
public:
    StorageGuard() = default;
    explicit StorageGuard(UInt32 size);
    StorageGuard(UInt32 size, StorageClass cls);
    ~StorageGuard();
    
    StorageGuard(const StorageGuard&) = delete;
    StorageGuard& operator=(const StorageGuard&) = delete;
    StorageGuard(StorageGuard&& other) noexcept;
    StorageGuard& operator=(StorageGuard&& other) noexcept;
    
    [[nodiscard]] void* get() const { return address_; }
    [[nodiscard]] UInt32 size() const { return size_; }
    [[nodiscard]] bool valid() const { return address_ != nullptr; }
    
    void* release();
    void reset();
    
    // Typed access
    template<typename T>
    [[nodiscard]] T* as() const { return static_cast<T*>(address_); }
};

// =============================================================================
// Utility Functions
// =============================================================================

// Initialize storage
void storage_init_zero(void* address, UInt32 size);
void storage_init_value(void* address, UInt32 size, Byte value);
void storage_copy(void* dest, const void* src, UInt32 size);
void storage_move(void* dest, const void* src, UInt32 size);

// Compare storage
[[nodiscard]] Int32 storage_compare(const void* a, const void* b, UInt32 size);
[[nodiscard]] bool storage_equal(const void* a, const void* b, UInt32 size);

} // namespace storage
} // namespace cics

#endif // CICS_STORAGE_CONTROL_HPP
