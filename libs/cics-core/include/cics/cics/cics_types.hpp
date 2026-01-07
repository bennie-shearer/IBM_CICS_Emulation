#include <cstring>
#pragma once

// =============================================================================
// CICS Emulation - Enhanced CICS Core Types
// Version: 3.4.6
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <functional>
#include <any>

namespace cics::cics {

// =============================================================================
// CICS Response Codes (EIBRESP)
// =============================================================================

enum class CicsResponse : UInt16 {
    NORMAL = 0,
    ERROR = 1,
    RDATT = 2,
    WRBRK = 3,
    EOF_ = 4,
    EODS = 5,
    EOC = 6,
    INBFMH = 7,
    ENDINPT = 8,
    NONVAL = 9,
    NOSTART = 10,
    TERMIDERR = 11,
    FILENOTFOUND = 12,
    NOTFND = 13,
    DUPREC = 14,
    DUPKEY = 15,
    INVREQ = 16,
    IOERR = 17,
    NOSPACE = 18,
    NOTOPEN = 19,
    ENDFILE = 20,
    ILLOGIC = 21,
    LENGERR = 22,
    QZERO = 23,
    SIGNAL = 24,
    QBUSY = 25,
    ITEMERR = 26,
    PGMIDERR = 27,
    TRANSIDERR = 28,
    ENDDATA = 29,
    INVTSREQ = 30,
    EXPIRED = 31,
    RETPAGE = 32,
    RTEFAIL = 33,
    RTESOME = 34,
    TSIOERR = 35,
    MAPFAIL = 36,
    INVERRTERM = 37,
    INVMPSZ = 38,
    IGREQID = 39,
    OVERFLOW = 40,
    INVLDC = 41,
    NOSTG = 42,
    JIDERR = 43,
    QIDERR = 44,
    NOJBUFSP = 45,
    DSSTAT = 46,
    SELNERR = 47,
    FUNCERR = 48,
    UNEXPIN = 49,
    NOPASSBKRD = 50,
    NOPASSBKWR = 51,
    SEGIDERR = 52,
    SYSIDERR = 53,
    ISCINVREQ = 54,
    ENQBUSY = 55,
    ENVDEFERR = 56,
    IGREQCD = 57,
    SESSIONERR = 58,
    SYSBUSY = 59,
    SESSBUSY = 60,
    NOTALLOC = 61,
    CBIDERR = 62,
    INVEXITREQ = 63,
    INVPARTNSET = 64,
    INVPARTN = 65,
    PARTNFAIL = 66,
    USERIDERR = 67,
    NOTAUTH = 68,
    VOLIDERR = 69,
    SUPPRESSED = 70,
    DISABLED = 84,
    ALLOCERR = 85,
    STRELERR = 86,
    OPENERR = 87,
    SPOLBUSY = 88,
    SPOLERR = 89,
    NODEIDERR = 90,
    TASKIDERR = 91,
    TABORNOTC = 92,
    ATNOTCONN = 93,
    ATIOTASKD = 94,
    LOADING = 94,
};

// =============================================================================
// CICS Response 2 Codes (EIBRESP2)
// =============================================================================

enum class CicsResponse2 : UInt16 {
    NORMAL = 0,
    // FILE CONTROL resp2 values
    FILE_DISABLED = 1,
    FILE_CLOSED = 2,
    FILE_LOADING = 3,
    // PROGRAM resp2 values
    PROGRAM_DISABLED = 1,
    PROGRAM_NOT_DEFINED = 2,
    // ... more as needed
};

// =============================================================================
// Execute Interface Block (EIB)
// =============================================================================

struct EIB {
    // Time and date
    UInt32 eibtime;           // Time in 0HHMMSS+ packed decimal
    UInt32 eibdate;           // Date in 0CYYDDD+ packed decimal
    
    // Transaction identification
    FixedString<4> eibtrnid;  // Transaction ID
    FixedString<4> eibtaskn;  // Task number
    FixedString<4> eibtrmid;  // Terminal ID
    
    // File control
    FixedString<8> eibfn;     // Function code
    CicsResponse eibresp;     // Response code
    CicsResponse2 eibresp2;   // Response2 code
    
    // Data
    UInt32 eibcalen;          // COMMAREA length
    FixedString<8> eibds;     // Dataset name
    FixedString<8> eibreqid;  // Request ID
    
    // Resource information
    FixedString<8> eibrsrce;  // Resource name
    
    // Cursor position
    UInt16 eibcposn;          // Cursor position
    
    // Attention ID
    UInt8 eibaid;             // Attention ID (PF key)
    
    // Flags
    bool eibatt;              // Attach pending
    bool eibeoc;              // End of chain
    bool eibfmh;              // FMH received
    bool eibcompl;            // Complete
    bool eibsig;              // Signal
    bool eibconf;             // Confirm
    bool eiberr;              // Error
    bool eibfree;             // Free
    bool eibrecv;             // Receive issued
    bool eibsend;             // Send issued
    bool eibsync;             // Sync point
    bool eibnodat;            // No data
    
    // Constructors
    EIB();
    void reset();
    void set_time_date();
    
    // Response helpers
    [[nodiscard]] bool is_normal() const { return eibresp == CicsResponse::NORMAL; }
    [[nodiscard]] bool is_error() const { return eibresp != CicsResponse::NORMAL; }
    [[nodiscard]] String response_name() const;
};

// =============================================================================
// COMMAREA (Communication Area)
// =============================================================================

class Commarea {
private:
    ByteBuffer data_;
    Size max_length_;
    
public:
    static constexpr Size MAX_COMMAREA_LENGTH = 32767;
    
    Commarea() : max_length_(MAX_COMMAREA_LENGTH) {}
    explicit Commarea(Size size) : data_(size), max_length_(MAX_COMMAREA_LENGTH) {}
    explicit Commarea(ConstByteSpan data) 
        : data_(data.begin(), data.end()), max_length_(MAX_COMMAREA_LENGTH) {}
    
    // Access
    [[nodiscard]] Byte* data() { return data_.data(); }
    [[nodiscard]] const Byte* data() const { return data_.data(); }
    [[nodiscard]] Size length() const { return data_.size(); }
    [[nodiscard]] Size capacity() const { return max_length_; }
    [[nodiscard]] bool empty() const { return data_.empty(); }
    
    [[nodiscard]] ByteSpan span() { return data_; }
    [[nodiscard]] ConstByteSpan span() const { return data_; }
    
    // Modify
    void resize(Size new_size);
    void clear() { data_.clear(); }
    void set_data(ConstByteSpan data);
    
    // String helpers
    void set_string(Size offset, StringView str, Size field_length);
    [[nodiscard]] String get_string(Size offset, Size length) const;
    
    // Numeric helpers
    template<Integral T>
    void set_value(Size offset, T value) {
        if (offset + sizeof(T) > data_.size()) {
            data_.resize(offset + sizeof(T));
        }
        std::memcpy(data_.data() + offset, &value, sizeof(T));
    }
    
    template<Integral T>
    [[nodiscard]] T get_value(Size offset) const {
        T value{};
        if (offset + sizeof(T) <= data_.size()) {
            std::memcpy(&value, data_.data() + offset, sizeof(T));
        }
        return value;
    }
};

// =============================================================================
// Transaction Definition
// =============================================================================

struct TransactionDefinition {
    FixedString<4> transaction_id;
    FixedString<8> program_name;
    String description;
    UInt16 priority = 100;
    UInt16 twasize = 0;           // Transaction Work Area size
    Duration timeout = std::chrono::minutes(5);
    bool enabled = true;
    bool dynamic = false;
    String profile;               // Transaction profile
    String security_key;
    
    // Resource limits
    UInt32 max_storage = 0;       // 0 = unlimited
    UInt32 max_runtime_seconds = 0;
    
    TransactionDefinition() = default;
    TransactionDefinition(StringView txn_id, StringView pgm_name);
};

// =============================================================================
// Program Definition
// =============================================================================

enum class ProgramLanguage : UInt8 {
    COBOL = 1,
    PLI = 2,
    ASSEMBLER = 3,
    C = 4,
    CPP = 5,
    JAVA = 6
};

struct ProgramDefinition {
    FixedString<8> program_name;
    ProgramLanguage language = ProgramLanguage::CPP;
    String description;
    UInt32 size = 0;
    bool enabled = true;
    bool resident = false;
    String library_name;
    
    // Usage statistics
    AtomicCounter<> use_count;
    SystemTimePoint last_used;
    
    ProgramDefinition() = default;
    explicit ProgramDefinition(StringView name);
};

// =============================================================================
// File Control Table (FCT) Entry
// =============================================================================

enum class FileType : UInt8 {
    KSDS = 1,
    ESDS = 2,
    RRDS = 3,
    PATH = 4,
    BDAM = 5
};

enum class FileAccess : UInt8 {
    READ = 1,
    WRITE = 2,
    UPDATE = 3,
    BROWSE = 4,
    DELETE = 8,
    ADD = 16
};

struct FileDefinition {
    FixedString<8> file_name;
    String dataset_name;
    FileType type = FileType::KSDS;
    UInt8 access_mode = 0;        // Bitmask of FileAccess
    UInt16 record_size = 0;
    UInt16 key_position = 0;
    UInt16 key_length = 0;
    bool enabled = true;
    bool browsable = true;
    String recovery_type;
    
    FileDefinition() = default;
    FileDefinition(StringView name, StringView dsn, FileType ft);
    
    bool has_access(FileAccess access) const {
        return (access_mode & static_cast<UInt8>(access)) != 0;
    }
};

// =============================================================================
// Transaction Status
// =============================================================================

enum class TransactionStatus : UInt8 {
    ACTIVE = 1,
    SUSPENDED = 2,
    WAITING = 3,
    RUNNING = 4,
    COMPLETED = 5,
    ABENDED = 6
};

// =============================================================================
// CICS Task
// =============================================================================

class CicsTask {
private:
    UInt32 task_number_;
    FixedString<4> transaction_id_;
    FixedString<4> terminal_id_;
    TransactionStatus status_;
    EIB eib_;
    Commarea commarea_;
    ByteBuffer twa_;              // Transaction Work Area
    SystemTimePoint start_time_;
    String user_id_;
    Duration cpu_time_;
    UInt32 storage_used_;
    SharedPtr<void> context_;     // Application-specific context
    
public:
    CicsTask(UInt32 task_num, StringView txn_id, StringView term_id = "");
    
    // Accessors
    [[nodiscard]] UInt32 task_number() const { return task_number_; }
    [[nodiscard]] const FixedString<4>& transaction_id() const { return transaction_id_; }
    [[nodiscard]] const FixedString<4>& terminal_id() const { return terminal_id_; }
    [[nodiscard]] TransactionStatus status() const { return status_; }
    [[nodiscard]] EIB& eib() { return eib_; }
    [[nodiscard]] const EIB& eib() const { return eib_; }
    [[nodiscard]] Commarea& commarea() { return commarea_; }
    [[nodiscard]] const Commarea& commarea() const { return commarea_; }
    [[nodiscard]] ByteSpan twa() { return twa_; }
    [[nodiscard]] ConstByteSpan twa() const { return twa_; }
    [[nodiscard]] SystemTimePoint start_time() const { return start_time_; }
    [[nodiscard]] Duration elapsed_time() const { 
        return std::chrono::duration_cast<Duration>(SystemClock::now() - start_time_); 
    }
    
    // Modifiers
    void set_status(TransactionStatus status) { status_ = status; }
    void set_user_id(const String& id) { user_id_ = id; }
    void resize_twa(Size size) { twa_.resize(size); }
    
    template<typename T>
    void set_context(SharedPtr<T> ctx) { context_ = std::static_pointer_cast<void>(ctx); }
    
    template<typename T>
    [[nodiscard]] SharedPtr<T> get_context() const { 
        return std::static_pointer_cast<T>(context_); 
    }
};

// =============================================================================
// CICS Command Types
// =============================================================================

enum class CicsCommand : UInt16 {
    // File Control
    READ = 0x0001,
    WRITE = 0x0002,
    REWRITE = 0x0003,
    DELETE_ = 0x0004,
    STARTBR = 0x0005,
    READNEXT = 0x0006,
    READPREV = 0x0007,
    ENDBR = 0x0008,
    RESETBR = 0x0009,
    UNLOCK = 0x000A,
    
    // Program Control
    LINK = 0x0100,
    XCTL = 0x0101,
    RETURN = 0x0102,
    LOAD = 0x0103,
    RELEASE = 0x0104,
    ABEND = 0x0105,
    HANDLE_ABEND = 0x0106,
    
    // Terminal Control
    SEND = 0x0200,
    RECEIVE = 0x0201,
    CONVERSE = 0x0202,
    SEND_MAP = 0x0203,
    RECEIVE_MAP = 0x0204,
    
    // Interval Control
    ASKTIME = 0x0300,
    FORMATTIME = 0x0301,
    START = 0x0302,
    RETRIEVE = 0x0303,
    CANCEL = 0x0304,
    DELAY = 0x0305,
    
    // Task Control
    SUSPEND = 0x0400,
    ENQ = 0x0401,
    DEQ = 0x0402,
    
    // Storage Control
    GETMAIN = 0x0500,
    FREEMAIN = 0x0501,
    
    // Temporary Storage
    WRITEQ_TS = 0x0600,
    READQ_TS = 0x0601,
    DELETEQ_TS = 0x0602,
    
    // Transient Data
    WRITEQ_TD = 0x0700,
    READQ_TD = 0x0701,
    DELETEQ_TD = 0x0702,
    
    // Sync Point
    SYNCPOINT = 0x0800,
    SYNCPOINT_ROLLBACK = 0x0801,
    
    // Dump and Trace
    DUMP = 0x0900,
    ENTER = 0x0901,
};

// =============================================================================
// CICS Command Options
// =============================================================================

struct CommandOptions {
    // Common options
    Optional<FixedString<8>> file;
    Optional<FixedString<8>> dataset;
    Optional<FixedString<8>> program;
    Optional<FixedString<8>> transid;
    Optional<FixedString<16>> queue;
    
    // Key/Record options
    ByteBuffer ridfld;            // Record ID field
    UInt16 keylength = 0;
    UInt16 length = 0;
    
    // Flags
    bool update = false;
    bool generic = false;
    bool gteq = false;
    bool equal = false;
    bool rba = false;
    bool rrn = false;
    bool from_data = false;
    bool into_data = false;
    bool set = false;
    bool nosuspend = false;
    bool nohandle = false;
    
    // Data pointers
    void* into = nullptr;
    const void* from = nullptr;
    
    // Interval options
    Optional<Duration> interval;
    Optional<SystemTimePoint> time;
    
    // Response capture
    CicsResponse* resp = nullptr;
    CicsResponse2* resp2 = nullptr;
};

// =============================================================================
// Program Handler Type
// =============================================================================

using ProgramHandler = std::function<CicsResponse(CicsTask&, Commarea&)>;

// =============================================================================
// CICS Statistics
// =============================================================================

struct CicsStatistics {
    AtomicCounter<> total_transactions;
    AtomicCounter<> successful_transactions;
    AtomicCounter<> failed_transactions;
    AtomicCounter<> abended_transactions;
    AtomicCounter<> active_tasks;
    AtomicCounter<> peak_tasks;
    
    AtomicCounter<UInt64> total_file_reads;
    AtomicCounter<UInt64> total_file_writes;
    AtomicCounter<UInt64> total_ts_operations;
    AtomicCounter<UInt64> total_td_operations;
    
    std::atomic<Int64> total_response_time_ms{0};
    Duration min_response_time = Duration::max();
    Duration max_response_time = Duration::zero();
    
    SystemTimePoint start_time = SystemClock::now();
    
    void record_transaction(Duration response_time, bool success, bool abend = false);
    void update_active_tasks(Int32 delta);
    
    [[nodiscard]] double average_response_ms() const;
    [[nodiscard]] double transactions_per_second() const;
    [[nodiscard]] double success_rate() const;
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_json() const;
};

// =============================================================================
// Utility Functions
// =============================================================================

[[nodiscard]] StringView response_name(CicsResponse resp);
[[nodiscard]] StringView command_name(CicsCommand cmd);
[[nodiscard]] StringView status_name(TransactionStatus status);

} // namespace cics::cics
