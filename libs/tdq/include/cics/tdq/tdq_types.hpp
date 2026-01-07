#pragma once

// =============================================================================
// CICS Emulation - Transient Data Queue (TDQ) Types
// Version: 3.4.6
// =============================================================================
//
// IBM CICS TDQ (Transient Data Queue) emulation for:
// - Automatic Transaction Initiation (ATI)
// - Extrapartition destinations (files/printers)
// - Intrapartition queuing
// - Indirect queue routing
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <map>
#include <shared_mutex>
#include <queue>
#include <fstream>
#include <functional>

namespace cics::tdq {

using namespace cics;

// =============================================================================
// Constants
// =============================================================================

constexpr Size MAX_DEST_NAME_LENGTH = 4;
constexpr Size MAX_RECORD_LENGTH = 32767;
constexpr Size DEFAULT_TRIGGER_LEVEL = 1;

// =============================================================================
// TDQ Types
// =============================================================================

enum class TDQType : UInt8 {
    INTRAPARTITION = 1,   // Memory-based, can trigger transactions
    EXTRAPARTITION = 2,   // File-based, for external I/O
    INDIRECT = 3,         // Pointer to another destination
    REMOTE = 4            // Remote system destination
};

[[nodiscard]] constexpr StringView to_string(TDQType type) {
    switch (type) {
        case TDQType::INTRAPARTITION: return "INTRAPARTITION";
        case TDQType::EXTRAPARTITION: return "EXTRAPARTITION";
        case TDQType::INDIRECT: return "INDIRECT";
        case TDQType::REMOTE: return "REMOTE";
    }
    return "UNKNOWN";
}

enum class TDQDisposition : UInt8 {
    REREAD = 1,    // Records can be re-read
    DELETE = 2     // Records deleted after read
};

enum class TDQOpenMode : UInt8 {
    INPUT = 1,
    OUTPUT = 2
};

// =============================================================================
// TDQ Return Codes
// =============================================================================

enum class TDQRC : UInt16 {
    OK = 0,
    QIDERR = 1,          // Queue not found
    LENGERR = 2,         // Length error
    NOSPACE = 3,         // No space available
    INVREQ = 4,          // Invalid request
    IOERR = 5,           // I/O error
    NOTOPEN = 6,         // Queue not open
    QZERO = 7,           // Queue is empty
    DISABLED = 8,        // Queue is disabled
    NOTAUTH = 9,         // Not authorized
    QBUSY = 10,          // Queue is busy
    SYSIDERR = 11,       // System ID error
    LOCKED = 12          // Queue is locked
};

[[nodiscard]] constexpr StringView to_string(TDQRC rc) {
    switch (rc) {
        case TDQRC::OK: return "OK";
        case TDQRC::QIDERR: return "QIDERR";
        case TDQRC::LENGERR: return "LENGERR";
        case TDQRC::NOSPACE: return "NOSPACE";
        case TDQRC::INVREQ: return "INVREQ";
        case TDQRC::IOERR: return "IOERR";
        case TDQRC::NOTOPEN: return "NOTOPEN";
        case TDQRC::QZERO: return "QZERO";
        case TDQRC::DISABLED: return "DISABLED";
        case TDQRC::NOTAUTH: return "NOTAUTH";
        case TDQRC::QBUSY: return "QBUSY";
        case TDQRC::SYSIDERR: return "SYSIDERR";
        case TDQRC::LOCKED: return "LOCKED";
    }
    return "UNKNOWN";
}

// =============================================================================
// TDQ Record
// =============================================================================

class TDQRecord {
private:
    ByteBuffer data_;
    UInt64 sequence_number_ = 0;
    SystemTimePoint timestamp_;
    String transaction_id_;
    String terminal_id_;
    
public:
    TDQRecord() : timestamp_(SystemClock::now()) {}
    
    explicit TDQRecord(ConstByteSpan data, UInt64 seq = 0)
        : data_(data.begin(), data.end())
        , sequence_number_(seq)
        , timestamp_(SystemClock::now()) {}
    
    explicit TDQRecord(StringView str, UInt64 seq = 0)
        : data_(str.begin(), str.end())
        , sequence_number_(seq)
        , timestamp_(SystemClock::now()) {}
    
    // Data access
    [[nodiscard]] Byte* data() { return data_.data(); }
    [[nodiscard]] const Byte* data() const { return data_.data(); }
    [[nodiscard]] Size length() const { return data_.size(); }
    [[nodiscard]] bool empty() const { return data_.empty(); }
    
    [[nodiscard]] ByteSpan span() { return data_; }
    [[nodiscard]] ConstByteSpan span() const { return data_; }
    
    // Sequence
    [[nodiscard]] UInt64 sequence_number() const { return sequence_number_; }
    void set_sequence_number(UInt64 seq) { sequence_number_ = seq; }
    
    // Timestamp
    [[nodiscard]] SystemTimePoint timestamp() const { return timestamp_; }
    
    // Transaction context
    [[nodiscard]] const String& transaction_id() const { return transaction_id_; }
    [[nodiscard]] const String& terminal_id() const { return terminal_id_; }
    void set_transaction_id(StringView txn) { transaction_id_ = String(txn); }
    void set_terminal_id(StringView term) { terminal_id_ = String(term); }
    
    // Conversion
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// Trigger Definition (for ATI)
// =============================================================================

struct TriggerDefinition {
    String transaction_id;       // Transaction to start
    UInt32 trigger_level = 1;    // Number of records to trigger
    String terminal_id;          // Optional terminal ID
    String user_id;              // Optional user ID
    bool enabled = true;
    
    using TriggerCallback = std::function<void(StringView transaction, StringView dest)>;
    TriggerCallback callback;
};

// =============================================================================
// TDQ Definition
// =============================================================================

struct TDQDefinition {
    FixedString<4> dest_id;
    TDQType type = TDQType::INTRAPARTITION;
    TDQDisposition disposition = TDQDisposition::DELETE;
    
    // Intrapartition options
    bool recoverable = false;
    bool reusable = false;
    Size max_records = 0;    // 0 = unlimited
    
    // Trigger (ATI) options
    Optional<TriggerDefinition> trigger;
    
    // Extrapartition options
    Path file_path;
    bool file_append = true;
    Size record_length = 0;  // 0 = variable
    
    // Indirect options
    String indirect_dest;
    
    // Remote options
    String remote_sysid;
    
    // Security
    String security_key;
    bool enabled = true;
    
    [[nodiscard]] Result<void> validate() const;
};

// =============================================================================
// TDQ Statistics
// =============================================================================

struct TDQStatistics {
    AtomicCounter<UInt64> current_depth;
    AtomicCounter<UInt64> total_records_written;
    AtomicCounter<UInt64> total_records_read;
    AtomicCounter<UInt64> total_bytes_written;
    AtomicCounter<UInt64> total_bytes_read;
    AtomicCounter<UInt64> trigger_count;
    UInt64 peak_depth = 0;
    SystemTimePoint created;
    SystemTimePoint last_write;
    SystemTimePoint last_read;
    
    TDQStatistics();
    
    void record_write(Size bytes);
    void record_read(Size bytes);
    void record_trigger();
    void update_peak_depth(UInt64 depth);
    
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_json() const;
};

// =============================================================================
// Intrapartition Queue
// =============================================================================

class IntrapartitionQueue {
private:
    TDQDefinition definition_;
    std::queue<TDQRecord> records_;
    TDQStatistics statistics_;
    mutable std::shared_mutex mutex_;
    UInt64 sequence_counter_ = 0;
    bool enabled_ = true;
    
    void check_trigger();
    
public:
    explicit IntrapartitionQueue(TDQDefinition def);
    ~IntrapartitionQueue() = default;
    
    // Write record
    Result<void> write(ConstByteSpan data);
    Result<void> write(StringView str);
    
    // Read record (destructive for DELETE disposition)
    Result<TDQRecord> read();
    
    // Peek without removing
    Result<TDQRecord> peek() const;
    
    // Queue info
    [[nodiscard]] const TDQDefinition& definition() const { return definition_; }
    [[nodiscard]] Size depth() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    
    // Statistics
    [[nodiscard]] const TDQStatistics& statistics() const { return statistics_; }
};

// =============================================================================
// Extrapartition Queue
// =============================================================================

class ExtrapartitionQueue {
private:
    TDQDefinition definition_;
    std::fstream file_;
    TDQStatistics statistics_;
    mutable std::mutex mutex_;
    TDQOpenMode open_mode_ = TDQOpenMode::OUTPUT;
    bool is_open_ = false;
    
public:
    explicit ExtrapartitionQueue(TDQDefinition def);
    ~ExtrapartitionQueue();
    
    // Open/Close
    Result<void> open(TDQOpenMode mode);
    Result<void> close();
    [[nodiscard]] bool is_open() const { return is_open_; }
    
    // Write record
    Result<void> write(ConstByteSpan data);
    Result<void> write(StringView str);
    
    // Read record
    Result<TDQRecord> read();
    
    // Queue info
    [[nodiscard]] const TDQDefinition& definition() const { return definition_; }
    [[nodiscard]] bool is_enabled() const { return definition_.enabled; }
    
    // Statistics
    [[nodiscard]] const TDQStatistics& statistics() const { return statistics_; }
};

// =============================================================================
// TDQ Manager
// =============================================================================

class TDQManager {
private:
    std::map<String, UniquePtr<IntrapartitionQueue>> intra_queues_;
    std::map<String, UniquePtr<ExtrapartitionQueue>> extra_queues_;
    std::map<String, String> indirect_map_;
    mutable std::shared_mutex mutex_;
    bool initialized_ = false;
    
    // Statistics
    AtomicCounter<UInt64> total_dests_defined_;
    AtomicCounter<UInt64> total_writes_;
    AtomicCounter<UInt64> total_reads_;
    
    // Resolve indirect destination
    Result<String> resolve_destination(StringView dest) const;
    
public:
    TDQManager();
    ~TDQManager();
    
    // Singleton access
    static TDQManager& instance();
    
    // Initialization
    Result<void> initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }
    
    // Destination definition
    Result<void> define_intrapartition(const TDQDefinition& def);
    Result<void> define_extrapartition(const TDQDefinition& def);
    Result<void> define_indirect(StringView dest, StringView target);
    Result<void> delete_destination(StringView dest);
    
    // WRITEQ TD
    Result<void> writeq(StringView dest, ConstByteSpan data);
    Result<void> writeq(StringView dest, StringView str);
    
    // READQ TD
    Result<TDQRecord> readq(StringView dest);
    
    // DELETEQ TD
    Result<void> deleteq(StringView dest);
    
    // Enable/Disable
    Result<void> enable_destination(StringView dest);
    Result<void> disable_destination(StringView dest);
    
    // Query
    [[nodiscard]] bool destination_exists(StringView dest) const;
    [[nodiscard]] Size destination_count() const;
    [[nodiscard]] std::vector<String> list_destinations() const;
    [[nodiscard]] Optional<TDQType> get_destination_type(StringView dest) const;
    [[nodiscard]] Result<Size> get_queue_depth(StringView dest) const;
    
    // Statistics
    [[nodiscard]] String get_statistics() const;
};

// =============================================================================
// CICS Command Interface
// =============================================================================

// EXEC CICS WRITEQ TD simulation
Result<void> exec_cics_writeq_td(
    StringView queue,
    ConstByteSpan from
);

// EXEC CICS READQ TD simulation
Result<ByteBuffer> exec_cics_readq_td(
    StringView queue
);

// EXEC CICS DELETEQ TD simulation
Result<void> exec_cics_deleteq_td(
    StringView queue
);

} // namespace cics::tdq
