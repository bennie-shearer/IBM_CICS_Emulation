#pragma once

// =============================================================================
// CICS Emulation - Enhanced VSAM Types
// Version: 3.4.6
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <map>
#include <shared_mutex>
#include <compare>
#include <cstring>

namespace cics::vsam {

// =============================================================================
// Constants
// =============================================================================

constexpr Size MIN_CI_SIZE = 512;
constexpr Size MAX_CI_SIZE = 32768;
constexpr Size DEFAULT_CI_SIZE = 4096;
constexpr Size MAX_KEY_LENGTH = 255;
constexpr Size MAX_RECORD_LENGTH = 32760;
constexpr Size DEFAULT_BUFFERS = 4;
constexpr UInt8 DEFAULT_FREE_CI_PERCENT = 10;
constexpr UInt8 DEFAULT_FREE_CA_PERCENT = 10;

// =============================================================================
// VSAM File Types
// =============================================================================

enum class VsamType : UInt8 {
    KSDS = 1,    // Key Sequenced Data Set
    ESDS = 2,    // Entry Sequenced Data Set
    RRDS = 3,    // Relative Record Data Set
    LDS = 4,     // Linear Data Set
    VRRDS = 5    // Variable-length RRDS
};

[[nodiscard]] constexpr StringView to_string(VsamType type) {
    switch (type) {
        case VsamType::KSDS: return "KSDS";
        case VsamType::ESDS: return "ESDS";
        case VsamType::RRDS: return "RRDS";
        case VsamType::LDS: return "LDS";
        case VsamType::VRRDS: return "VRRDS";
    }
    return "UNKNOWN";
}

// =============================================================================
// Access Modes
// =============================================================================

enum class AccessMode : UInt8 {
    INPUT = 1,      // Read-only
    OUTPUT = 2,     // Write-only (new records)
    IO = 3,         // Read/Write (update)
    EXTEND = 4      // Append
};

enum class ProcessingMode : UInt8 {
    SEQUENTIAL = 1,
    RANDOM = 2,
    DYNAMIC = 3,    // Sequential + Random
    SKIP = 4        // Skip-sequential
};

// =============================================================================
// VSAM Return Codes
// =============================================================================

enum class VsamRC : UInt16 {
    OK = 0,
    DUPLICATE_KEY = 8,
    SEQUENCE_ERROR = 12,
    RECORD_NOT_FOUND = 16,
    END_OF_FILE = 20,
    RBA_NOT_FOUND = 24,
    INVALID_REQUEST = 28,
    LOGIC_ERROR = 32,
    OUT_OF_SPACE = 36,
    NOT_OPEN = 40,
    ALREADY_OPEN = 44,
    KEY_CHANGE = 48,
    INVALID_KEY_LENGTH = 52,
    RECORD_TOO_LARGE = 56,
    CI_SPLIT = 60,
    CA_SPLIT = 64,
    PHYSICAL_ERROR = 96,
    INTERNAL_ERROR = 100
};

[[nodiscard]] constexpr StringView to_string(VsamRC rc) {
    switch (rc) {
        case VsamRC::OK: return "OK";
        case VsamRC::DUPLICATE_KEY: return "DUPLICATE_KEY";
        case VsamRC::SEQUENCE_ERROR: return "SEQUENCE_ERROR";
        case VsamRC::RECORD_NOT_FOUND: return "RECORD_NOT_FOUND";
        case VsamRC::END_OF_FILE: return "END_OF_FILE";
        case VsamRC::RBA_NOT_FOUND: return "RBA_NOT_FOUND";
        case VsamRC::INVALID_REQUEST: return "INVALID_REQUEST";
        case VsamRC::LOGIC_ERROR: return "LOGIC_ERROR";
        case VsamRC::OUT_OF_SPACE: return "OUT_OF_SPACE";
        case VsamRC::NOT_OPEN: return "NOT_OPEN";
        case VsamRC::ALREADY_OPEN: return "ALREADY_OPEN";
        case VsamRC::KEY_CHANGE: return "KEY_CHANGE";
        case VsamRC::INVALID_KEY_LENGTH: return "INVALID_KEY_LENGTH";
        case VsamRC::RECORD_TOO_LARGE: return "RECORD_TOO_LARGE";
        case VsamRC::CI_SPLIT: return "CI_SPLIT";
        case VsamRC::CA_SPLIT: return "CA_SPLIT";
        case VsamRC::PHYSICAL_ERROR: return "PHYSICAL_ERROR";
        case VsamRC::INTERNAL_ERROR: return "INTERNAL_ERROR";
    }
    return "UNKNOWN";
}

// =============================================================================
// VSAM Key
// =============================================================================

class VsamKey {
private:
    ByteBuffer data_;
    
public:
    VsamKey() = default;
    explicit VsamKey(Size length) : data_(length) {}
    explicit VsamKey(ConstByteSpan data) : data_(data.begin(), data.end()) {}
    explicit VsamKey(StringView str) : data_(str.begin(), str.end()) {}
    
    // Access
    [[nodiscard]] Byte* data() { return data_.data(); }
    [[nodiscard]] const Byte* data() const { return data_.data(); }
    [[nodiscard]] Size length() const { return data_.size(); }
    [[nodiscard]] bool empty() const { return data_.empty(); }
    
    [[nodiscard]] ByteSpan span() { return data_; }
    [[nodiscard]] ConstByteSpan span() const { return data_; }
    
    // Modify
    void resize(Size new_size) { data_.resize(new_size); }
    void clear() { data_.clear(); }
    void set(ConstByteSpan data) { data_.assign(data.begin(), data.end()); }
    void set(StringView str) { data_.assign(str.begin(), str.end()); }
    
    // Conversion
    [[nodiscard]] String to_string() const { 
        return String(reinterpret_cast<const char*>(data_.data()), data_.size()); 
    }
    [[nodiscard]] String to_hex() const;
    
    // Comparison - explicit handling avoids GCC -Wstringop-overread warning
    [[nodiscard]] std::strong_ordering operator<=>(const VsamKey& other) const {
        // Handle empty cases explicitly to avoid GCC memcmp bounds warning
        const bool this_empty = data_.empty();
        const bool other_empty = other.data_.empty();
        
        if (this_empty && other_empty) return std::strong_ordering::equal;
        if (this_empty) return std::strong_ordering::less;
        if (other_empty) return std::strong_ordering::greater;
        
        // Safe comparison when both have data
        const Size min_len = std::min(data_.size(), other.data_.size());
        const int cmp = std::memcmp(data_.data(), other.data_.data(), min_len);
        
        if (cmp < 0) return std::strong_ordering::less;
        if (cmp > 0) return std::strong_ordering::greater;
        
        // Equal prefix - shorter key is less
        if (data_.size() < other.data_.size()) return std::strong_ordering::less;
        if (data_.size() > other.data_.size()) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }
    
    [[nodiscard]] bool operator==(const VsamKey& other) const {
        return data_ == other.data_;
    }
    
    // Prefix comparison
    [[nodiscard]] bool starts_with(const VsamKey& prefix) const {
        if (prefix.length() > length()) return false;
        return std::equal(prefix.data_.begin(), prefix.data_.end(), data_.begin());
    }
};

// =============================================================================
// Relative Byte Address (RBA) and Relative Record Number (RRN)
// =============================================================================

using RBA = UInt64;
using RRN = UInt32;

constexpr RBA INVALID_RBA = UINT64_MAX;
constexpr RRN INVALID_RRN = UINT32_MAX;

struct VsamAddress {
    RBA rba = INVALID_RBA;
    RRN rrn = INVALID_RRN;
    UInt32 ci_number = 0;
    UInt16 slot_number = 0;
    
    [[nodiscard]] bool is_valid() const { 
        return rba != INVALID_RBA || rrn != INVALID_RRN; 
    }
    [[nodiscard]] bool has_rba() const { return rba != INVALID_RBA; }
    [[nodiscard]] bool has_rrn() const { return rrn != INVALID_RRN; }
    
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// VSAM Record
// =============================================================================

class VsamRecord {
private:
    VsamKey key_;
    ByteBuffer data_;
    VsamAddress address_;
    SystemTimePoint last_modified_;
    bool deleted_ = false;
    
public:
    VsamRecord() : last_modified_(SystemClock::now()) {}
    VsamRecord(VsamKey key, ConstByteSpan data);
    VsamRecord(ConstByteSpan data, RBA rba);  // For ESDS
    VsamRecord(ConstByteSpan data, RRN rrn);  // For RRDS
    
    // Key access (KSDS)
    [[nodiscard]] VsamKey& key() { return key_; }
    [[nodiscard]] const VsamKey& key() const { return key_; }
    void set_key(VsamKey key) { key_ = std::move(key); }
    
    // Data access
    [[nodiscard]] Byte* data() { return data_.data(); }
    [[nodiscard]] const Byte* data() const { return data_.data(); }
    [[nodiscard]] Size length() const { return data_.size(); }
    [[nodiscard]] bool empty() const { return data_.empty(); }
    
    [[nodiscard]] ByteSpan span() { return data_; }
    [[nodiscard]] ConstByteSpan span() const { return data_; }
    
    void set_data(ConstByteSpan data);
    void resize(Size new_size) { data_.resize(new_size); }
    
    // Address
    [[nodiscard]] const VsamAddress& address() const { return address_; }
    void set_address(VsamAddress addr) { address_ = addr; }
    [[nodiscard]] RBA rba() const { return address_.rba; }
    [[nodiscard]] RRN rrn() const { return address_.rrn; }
    
    // Status
    [[nodiscard]] SystemTimePoint last_modified() const { return last_modified_; }
    [[nodiscard]] bool is_deleted() const { return deleted_; }
    void mark_deleted() { deleted_ = true; last_modified_ = SystemClock::now(); }
    void mark_active() { deleted_ = false; last_modified_ = SystemClock::now(); }
    [[nodiscard]] bool is_valid() const { return !deleted_ && !data_.empty(); }
    
    // Total size (for storage calculations)
    [[nodiscard]] Size total_size() const { 
        return key_.length() + data_.size() + sizeof(VsamAddress) + sizeof(bool); 
    }
    
    // Serialization
    [[nodiscard]] ByteBuffer serialize() const;
    static VsamRecord deserialize(ConstByteSpan data);
};

// =============================================================================
// Control Interval Definition
// =============================================================================

struct ControlInterval {
    UInt32 ci_number;
    UInt16 ci_size;
    UInt16 free_space;
    UInt16 record_count;
    std::vector<UInt16> rdf;  // Record Definition Fields (offsets)
    ByteBuffer data;
    
    ControlInterval() = default;
    explicit ControlInterval(UInt32 num, UInt16 size);
    
    [[nodiscard]] bool has_space_for(Size record_size) const;
    [[nodiscard]] double utilization() const;
    void clear();
};

// =============================================================================
// Control Area Definition
// =============================================================================

struct ControlArea {
    UInt32 ca_number;
    UInt16 ci_count;
    UInt16 ci_per_ca;
    UInt32 record_count;
    UInt64 total_bytes;
    std::vector<ControlInterval> intervals;
    
    ControlArea() = default;
    ControlArea(UInt32 num, UInt16 ci_count, UInt16 ci_size);
    
    [[nodiscard]] Size free_space() const;
    [[nodiscard]] double utilization() const;
};

// =============================================================================
// VSAM File Definition
// =============================================================================

struct VsamDefinition {
    String cluster_name;
    String data_name;
    String index_name;
    VsamType type = VsamType::KSDS;
    
    // Key parameters (KSDS)
    UInt16 key_length = 0;
    UInt16 key_offset = 0;
    bool unique_key = true;
    
    // Record parameters
    UInt32 average_record_length = 100;
    UInt32 maximum_record_length = MAX_RECORD_LENGTH;
    bool spanned_records = false;
    
    // Space parameters
    UInt16 ci_size = DEFAULT_CI_SIZE;
    UInt16 ca_size = 1;  // Number of CIs per CA
    UInt8 free_ci_percent = DEFAULT_FREE_CI_PERCENT;
    UInt8 free_ca_percent = DEFAULT_FREE_CA_PERCENT;
    
    // Buffer parameters
    UInt16 data_buffers = DEFAULT_BUFFERS;
    UInt16 index_buffers = DEFAULT_BUFFERS;
    
    // Options
    bool recovery = false;
    bool reuse = false;
    bool erase = false;
    bool write_check = false;
    
    // Share options
    UInt8 cross_region = 1;
    UInt8 cross_system = 3;
    
    // SMS classes
    String storage_class;
    String management_class;
    String data_class;
    
    // Validation
    [[nodiscard]] Result<void> validate() const;
};

// =============================================================================
// VSAM Statistics
// =============================================================================

struct VsamStatistics {
    // Record counts
    AtomicCounter<UInt64> record_count;
    AtomicCounter<UInt64> deleted_records;
    
    // Space usage
    AtomicCounter<UInt64> total_bytes;
    AtomicCounter<UInt64> used_bytes;
    UInt64 allocated_bytes = 0;
    
    // CI/CA statistics
    UInt32 ci_count = 0;
    UInt32 ca_count = 0;
    AtomicCounter<> ci_splits;
    AtomicCounter<> ca_splits;
    
    // Index statistics (KSDS)
    UInt32 index_levels = 0;
    UInt32 index_records = 0;
    UInt64 index_bytes = 0;
    
    // I/O statistics
    AtomicCounter<UInt64> reads;
    AtomicCounter<UInt64> writes;
    AtomicCounter<UInt64> deletes;
    AtomicCounter<UInt64> updates;
    AtomicCounter<UInt64> inserts;
    AtomicCounter<UInt64> browses;
    
    // Performance
    std::atomic<Int64> total_io_time_ns{0};
    Duration min_io_time = Duration::max();
    Duration max_io_time = Duration::zero();
    
    // Timestamps
    SystemTimePoint created;
    SystemTimePoint last_accessed;
    SystemTimePoint last_modified;
    
    VsamStatistics();
    
    void record_read(Duration time);
    void record_write(Duration time, Size bytes);
    void record_delete();
    void record_update(Duration time);
    
    [[nodiscard]] double space_utilization() const;
    [[nodiscard]] double average_io_time_us() const;
    [[nodiscard]] double io_per_second() const;
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_json() const;
};

// =============================================================================
// Browse Context
// =============================================================================

class BrowseContext {
private:
    String browse_id_;
    VsamKey current_key_;
    VsamAddress current_address_;
    ProcessingMode mode_;
    bool backward_ = false;
    bool at_start_ = true;
    bool at_end_ = false;
    UInt64 records_read_ = 0;
    TimePoint start_time_;
    
public:
    BrowseContext();
    
    [[nodiscard]] const String& id() const { return browse_id_; }
    [[nodiscard]] const VsamKey& current_key() const { return current_key_; }
    [[nodiscard]] const VsamAddress& current_address() const { return current_address_; }
    [[nodiscard]] ProcessingMode mode() const { return mode_; }
    [[nodiscard]] bool is_backward() const { return backward_; }
    [[nodiscard]] bool at_start() const { return at_start_; }
    [[nodiscard]] bool at_end() const { return at_end_; }
    [[nodiscard]] UInt64 records_read() const { return records_read_; }
    
    void set_current(const VsamKey& key, const VsamAddress& addr);
    void set_mode(ProcessingMode mode) { mode_ = mode; }
    void set_backward(bool backward) { backward_ = backward; }
    void set_at_start(bool at_start) { at_start_ = at_start; }
    void set_at_end(bool at_end) { at_end_ = at_end; }
    void increment_records() { ++records_read_; }
    void reset();
};

// =============================================================================
// VSAM Request/Response
// =============================================================================

enum class VsamOperation : UInt8 {
    OPEN = 1,
    CLOSE = 2,
    GET = 3,
    PUT = 4,
    ERASE = 5,
    POINT = 6,
    STARTBR = 7,
    READNEXT = 8,
    READPREV = 9,
    ENDBR = 10,
    VERIFY = 11
};

struct VsamRequest {
    VsamOperation operation;
    VsamKey key;
    ByteBuffer data;
    VsamAddress address;
    ProcessingMode mode = ProcessingMode::RANDOM;
    AccessMode access = AccessMode::INPUT;
    
    // Options
    bool generic = false;
    bool gteq = false;
    bool update = false;
    
    VsamRequest() = default;
    explicit VsamRequest(VsamOperation op) : operation(op) {}
};

struct VsamResponse {
    VsamRC return_code = VsamRC::OK;
    String message;
    Optional<VsamRecord> record;
    VsamAddress next_address;
    Duration processing_time = Duration::zero();
    
    [[nodiscard]] bool is_ok() const { return return_code == VsamRC::OK; }
    [[nodiscard]] bool is_eof() const { return return_code == VsamRC::END_OF_FILE; }
    [[nodiscard]] bool not_found() const { return return_code == VsamRC::RECORD_NOT_FOUND; }
};

// =============================================================================
// VSAM File Interface
// =============================================================================

class IVsamFile {
public:
    virtual ~IVsamFile() = default;
    
    // File operations
    virtual Result<void> open(AccessMode mode, ProcessingMode proc = ProcessingMode::DYNAMIC) = 0;
    virtual Result<void> close() = 0;
    [[nodiscard]] virtual bool is_open() const = 0;
    
    // Record operations
    virtual Result<VsamRecord> read(const VsamKey& key) = 0;
    virtual Result<VsamRecord> read_by_rba(RBA rba) = 0;
    virtual Result<VsamRecord> read_by_rrn(RRN rrn) = 0;
    virtual Result<void> write(const VsamRecord& record) = 0;
    virtual Result<void> update(const VsamRecord& record) = 0;
    virtual Result<void> erase(const VsamKey& key) = 0;
    
    // Browse operations
    virtual Result<String> start_browse(const VsamKey& key, bool gteq = false, bool backward = false) = 0;
    virtual Result<VsamRecord> read_next(const String& browse_id) = 0;
    virtual Result<VsamRecord> read_prev(const String& browse_id) = 0;
    virtual Result<void> end_browse(const String& browse_id) = 0;
    virtual Result<void> reset_browse(const String& browse_id, const VsamKey& key) = 0;
    
    // Information
    [[nodiscard]] virtual const VsamDefinition& definition() const = 0;
    [[nodiscard]] virtual const VsamStatistics& statistics() const = 0;
    [[nodiscard]] virtual VsamType type() const = 0;
    [[nodiscard]] virtual UInt64 record_count() const = 0;
};

// =============================================================================
// Factory Functions
// =============================================================================

[[nodiscard]] UniquePtr<IVsamFile> create_vsam_file(const VsamDefinition& def, const Path& path);
[[nodiscard]] Result<UniquePtr<IVsamFile>> open_vsam_file(const Path& path, AccessMode mode);

} // namespace cics::vsam

// Hash support
template<>
struct std::hash<cics::vsam::VsamKey> {
    std::size_t operator()(const cics::vsam::VsamKey& k) const noexcept {
        return cics::fnv1a_hash(k.span());
    }
};
