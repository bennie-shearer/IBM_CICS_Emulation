#pragma once

// =============================================================================
// CICS Emulation - Temporary Storage Queue (TSQ) Types
// Version: 3.4.6
// =============================================================================
// 
// IBM CICS TSQ (Temporary Storage Queue) emulation for:
// - Scratchpad data between transactions
// - Pseudo-conversational program communication  
// - BMS map data storage
// - Intertransaction data passing
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <map>
#include <shared_mutex>
#include <deque>
#include <compare>

namespace cics::tsq {

using namespace cics;

// =============================================================================
// Constants
// =============================================================================

constexpr Size MAX_QUEUE_NAME_LENGTH = 16;
constexpr Size MAX_ITEM_LENGTH = 32767;
constexpr Size MAX_QUEUE_ITEMS = 32767;
constexpr Size DEFAULT_AUXILIARY_THRESHOLD = 100;

// =============================================================================
// TSQ Location
// =============================================================================

enum class TSQLocation : UInt8 {
    MAIN = 1,       // Main storage (memory)
    AUXILIARY = 2   // Auxiliary storage (disk)
};

[[nodiscard]] constexpr StringView to_string(TSQLocation loc) {
    switch (loc) {
        case TSQLocation::MAIN: return "MAIN";
        case TSQLocation::AUXILIARY: return "AUXILIARY";
    }
    return "UNKNOWN";
}

// =============================================================================
// TSQ Return Codes
// =============================================================================

enum class TSQRC : UInt16 {
    OK = 0,
    QIDERR = 1,          // Queue not found
    ITEMERR = 2,         // Item number out of range
    LENGERR = 3,         // Length error
    NOSPACE = 4,         // No space available
    INVREQ = 5,          // Invalid request
    IOERR = 6,           // I/O error
    LOCKED = 7,          // Queue is locked
    NOTAUTH = 8,         // Not authorized
    SYSIDERR = 9,        // System ID error
    ISCINVREQ = 10,      // ISC invalid request
    NOTOPEN = 11,        // Queue not open
    SUPPRESSED = 12      // Operation suppressed
};

[[nodiscard]] constexpr StringView to_string(TSQRC rc) {
    switch (rc) {
        case TSQRC::OK: return "OK";
        case TSQRC::QIDERR: return "QIDERR";
        case TSQRC::ITEMERR: return "ITEMERR";
        case TSQRC::LENGERR: return "LENGERR";
        case TSQRC::NOSPACE: return "NOSPACE";
        case TSQRC::INVREQ: return "INVREQ";
        case TSQRC::IOERR: return "IOERR";
        case TSQRC::LOCKED: return "LOCKED";
        case TSQRC::NOTAUTH: return "NOTAUTH";
        case TSQRC::SYSIDERR: return "SYSIDERR";
        case TSQRC::ISCINVREQ: return "ISCINVREQ";
        case TSQRC::NOTOPEN: return "NOTOPEN";
        case TSQRC::SUPPRESSED: return "SUPPRESSED";
    }
    return "UNKNOWN";
}

// =============================================================================
// TSQ Item
// =============================================================================

class TSQItem {
private:
    ByteBuffer data_;
    UInt32 item_number_ = 0;
    SystemTimePoint created_;
    SystemTimePoint last_modified_;
    String transaction_id_;
    String terminal_id_;
    
public:
    TSQItem() : created_(SystemClock::now()), last_modified_(created_) {}
    
    explicit TSQItem(ConstByteSpan data, UInt32 item_num = 0)
        : data_(data.begin(), data.end())
        , item_number_(item_num)
        , created_(SystemClock::now())
        , last_modified_(created_) {}
    
    explicit TSQItem(StringView str, UInt32 item_num = 0)
        : data_(str.begin(), str.end())
        , item_number_(item_num)
        , created_(SystemClock::now())
        , last_modified_(created_) {}
    
    // Data access
    [[nodiscard]] Byte* data() { return data_.data(); }
    [[nodiscard]] const Byte* data() const { return data_.data(); }
    [[nodiscard]] Size length() const { return data_.size(); }
    [[nodiscard]] bool empty() const { return data_.empty(); }
    
    [[nodiscard]] ByteSpan span() { return data_; }
    [[nodiscard]] ConstByteSpan span() const { return data_; }
    
    void set_data(ConstByteSpan data);
    void set_data(StringView str);
    
    // Item number
    [[nodiscard]] UInt32 item_number() const { return item_number_; }
    void set_item_number(UInt32 num) { item_number_ = num; }
    
    // Timestamps
    [[nodiscard]] SystemTimePoint created() const { return created_; }
    [[nodiscard]] SystemTimePoint last_modified() const { return last_modified_; }
    void touch() { last_modified_ = SystemClock::now(); }
    
    // Transaction context
    [[nodiscard]] const String& transaction_id() const { return transaction_id_; }
    [[nodiscard]] const String& terminal_id() const { return terminal_id_; }
    void set_transaction_id(StringView txn) { transaction_id_ = String(txn); }
    void set_terminal_id(StringView term) { terminal_id_ = String(term); }
    
    // Conversion
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// TSQ Definition
// =============================================================================

struct TSQDefinition {
    FixedString<16> queue_name;
    TSQLocation location = TSQLocation::MAIN;
    Size max_items = MAX_QUEUE_ITEMS;
    Size max_item_length = MAX_ITEM_LENGTH;
    bool recoverable = false;
    bool shared = false;
    String security_key;
    String owning_transaction;
    
    [[nodiscard]] Result<void> validate() const;
};

// =============================================================================
// TSQ Statistics
// =============================================================================

struct TSQStatistics {
    AtomicCounter<UInt64> total_items;
    AtomicCounter<UInt64> total_bytes;
    AtomicCounter<UInt64> reads;
    AtomicCounter<UInt64> writes;
    AtomicCounter<UInt64> rewrites;
    AtomicCounter<UInt64> deletes;
    AtomicCounter<UInt64> deleteqs;
    UInt64 peak_items = 0;
    UInt64 peak_bytes = 0;
    SystemTimePoint created;
    SystemTimePoint last_accessed;
    
    TSQStatistics();
    
    void record_read();
    void record_write(Size bytes);
    void record_rewrite(Size old_bytes, Size new_bytes);
    void record_delete(Size bytes);
    void update_peaks(UInt64 items, UInt64 bytes);
    
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_json() const;
};

// =============================================================================
// Temporary Storage Queue
// =============================================================================

class TemporaryStorageQueue {
private:
    TSQDefinition definition_;
    std::deque<TSQItem> items_;
    TSQStatistics statistics_;
    mutable std::shared_mutex mutex_;
    bool deleted_ = false;
    
public:
    explicit TemporaryStorageQueue(TSQDefinition def);
    ~TemporaryStorageQueue() = default;
    
    // Non-copyable, movable
    TemporaryStorageQueue(const TemporaryStorageQueue&) = delete;
    TemporaryStorageQueue& operator=(const TemporaryStorageQueue&) = delete;
    TemporaryStorageQueue(TemporaryStorageQueue&&) noexcept = default;
    TemporaryStorageQueue& operator=(TemporaryStorageQueue&&) noexcept = default;
    
    // WRITEQ TS - Write item to queue
    Result<UInt32> write(ConstByteSpan data);
    Result<UInt32> write(StringView str);
    
    // WRITEQ TS REWRITE - Rewrite existing item
    Result<void> rewrite(UInt32 item_number, ConstByteSpan data);
    Result<void> rewrite(UInt32 item_number, StringView str);
    
    // READQ TS - Read item from queue
    Result<TSQItem> read(UInt32 item_number) const;
    Result<TSQItem> read_next(UInt32& current_item) const;
    
    // DELETEQ TS - Delete single item
    Result<void> delete_item(UInt32 item_number);
    
    // DELETEQ TS (entire queue)
    Result<void> delete_all();
    
    // Queue information
    [[nodiscard]] const TSQDefinition& definition() const { return definition_; }
    [[nodiscard]] StringView name() const { return definition_.queue_name.trimmed(); }
    [[nodiscard]] TSQLocation location() const { return definition_.location; }
    [[nodiscard]] Size item_count() const;
    [[nodiscard]] Size total_bytes() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] bool is_deleted() const { return deleted_; }
    
    // Statistics
    [[nodiscard]] const TSQStatistics& statistics() const { return statistics_; }
};

// =============================================================================
// TSQ Manager
// =============================================================================

class TSQManager {
private:
    std::map<String, UniquePtr<TemporaryStorageQueue>> queues_;
    mutable std::shared_mutex mutex_;
    Size auxiliary_threshold_;
    Path auxiliary_storage_path_;
    bool initialized_ = false;
    
    // Statistics
    AtomicCounter<UInt64> total_queues_created_;
    AtomicCounter<UInt64> total_queues_deleted_;
    AtomicCounter<UInt64> active_queues_;
    
public:
    TSQManager();
    ~TSQManager();
    
    // Singleton access
    static TSQManager& instance();
    
    // Initialization
    Result<void> initialize(const Path& auxiliary_path = "");
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }
    
    // Queue operations
    Result<TemporaryStorageQueue*> get_queue(StringView name);
    Result<TemporaryStorageQueue*> get_or_create_queue(StringView name, 
                                                        TSQLocation location = TSQLocation::MAIN);
    Result<void> delete_queue(StringView name);
    [[nodiscard]] bool queue_exists(StringView name) const;
    
    // WRITEQ TS
    Result<UInt32> writeq(StringView queue_name, ConstByteSpan data,
                          TSQLocation location = TSQLocation::MAIN);
    Result<UInt32> writeq(StringView queue_name, StringView str,
                          TSQLocation location = TSQLocation::MAIN);
    
    // WRITEQ TS REWRITE
    Result<void> rewriteq(StringView queue_name, UInt32 item_number, ConstByteSpan data);
    Result<void> rewriteq(StringView queue_name, UInt32 item_number, StringView str);
    
    // READQ TS
    Result<TSQItem> readq(StringView queue_name, UInt32 item_number) const;
    Result<TSQItem> readq_next(StringView queue_name, UInt32& current_item) const;
    
    // DELETEQ TS
    Result<void> deleteq_item(StringView queue_name, UInt32 item_number);
    Result<void> deleteq(StringView queue_name);
    
    // Query
    [[nodiscard]] Size queue_count() const;
    [[nodiscard]] std::vector<String> list_queues() const;
    [[nodiscard]] std::vector<String> list_queues_by_prefix(StringView prefix) const;
    
    // Configuration
    void set_auxiliary_threshold(Size threshold) { auxiliary_threshold_ = threshold; }
    [[nodiscard]] Size auxiliary_threshold() const { return auxiliary_threshold_; }
    
    // Statistics
    [[nodiscard]] String get_statistics() const;
};

// =============================================================================
// CICS Command Interface
// =============================================================================

// EXEC CICS WRITEQ TS simulation
Result<UInt32> exec_cics_writeq_ts(
    StringView queue,
    ConstByteSpan from,
    TSQLocation location = TSQLocation::MAIN,
    bool rewrite = false,
    UInt32 item = 0
);

// EXEC CICS READQ TS simulation
Result<ByteBuffer> exec_cics_readq_ts(
    StringView queue,
    UInt32 item = 0,
    bool next = false
);

// EXEC CICS DELETEQ TS simulation
Result<void> exec_cics_deleteq_ts(
    StringView queue,
    Optional<UInt32> item = std::nullopt
);

} // namespace cics::tsq
