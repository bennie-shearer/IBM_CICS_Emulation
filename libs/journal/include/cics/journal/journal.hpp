// =============================================================================
// CICS Emulation - Journal Control
// =============================================================================
// Provides WRITE JOURNALNAME functionality for audit logging.
// Implements journal management for transaction recovery and audit trails.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#ifndef CICS_JOURNAL_HPP
#define CICS_JOURNAL_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <fstream>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue>

namespace cics {
namespace journal {

// =============================================================================
// Constants
// =============================================================================

constexpr UInt32 MAX_JOURNAL_NAME = 8;
constexpr UInt32 MAX_RECORD_LENGTH = 65535;
constexpr UInt32 DEFAULT_BUFFER_SIZE = 32768;
constexpr UInt32 DFHLOG = 1;    // System log journal number
constexpr UInt32 DFHSHUNT = 2; // Shunted UOW journal

// =============================================================================
// Enumerations
// =============================================================================

enum class JournalType {
    USER,           // User journal (01-99)
    SYSTEM,         // System journal (DFHLOG, DFHSHUNT)
    AUTO            // Auto journal
};

enum class JournalStatus {
    OPEN,
    CLOSED,
    FULL,
    ERROR
};

enum class WriteType {
    WAIT,           // Synchronous write
    NOWAIT,         // Asynchronous write
    STARTIO         // Start I/O but don't wait
};

// =============================================================================
// Journal Record
// =============================================================================

struct JournalRecord {
    UInt64 sequence_number;
    std::chrono::system_clock::time_point timestamp;
    String journal_name;
    String jtypeid;             // Journal type identifier
    String prefix;              // Prefix data
    ByteBuffer data;
    UInt32 length;
    String transaction_id;
    UInt32 task_id;
};

// =============================================================================
// Journal Information
// =============================================================================

struct JournalInfo {
    String name;
    UInt32 number;
    JournalType type;
    JournalStatus status;
    UInt64 records_written;
    UInt64 bytes_written;
    std::chrono::system_clock::time_point opened;
    std::chrono::system_clock::time_point last_write;
    String filename;
};

// =============================================================================
// Journal Class
// =============================================================================

class Journal {
public:
    Journal(StringView name, UInt32 number, JournalType type);
    ~Journal();
    
    // Properties
    [[nodiscard]] const String& name() const { return name_; }
    [[nodiscard]] UInt32 number() const { return number_; }
    [[nodiscard]] JournalType type() const { return type_; }
    [[nodiscard]] JournalStatus status() const { return status_; }
    [[nodiscard]] bool is_open() const { return status_ == JournalStatus::OPEN; }
    
    // Operations
    Result<void> open(StringView filename);
    Result<void> close();
    Result<UInt64> write(const JournalRecord& record);
    Result<UInt64> write(StringView jtypeid, const void* data, UInt32 length);
    Result<void> flush();
    
    // Information
    [[nodiscard]] JournalInfo get_info() const;
    [[nodiscard]] UInt64 records_written() const { return records_written_; }
    [[nodiscard]] UInt64 bytes_written() const { return bytes_written_; }
    
private:
    void write_header(std::ostream& out, const JournalRecord& record);
    
    String name_;
    UInt32 number_;
    JournalType type_;
    JournalStatus status_ = JournalStatus::CLOSED;
    
    std::ofstream file_;
    String filename_;
    
    std::atomic<UInt64> sequence_{0};
    std::atomic<UInt64> records_written_{0};
    std::atomic<UInt64> bytes_written_{0};
    std::chrono::system_clock::time_point opened_;
    std::chrono::system_clock::time_point last_write_;
    
    mutable std::mutex mutex_;
};

// =============================================================================
// Journal Statistics
// =============================================================================

struct JournalStats {
    UInt64 journals_opened{0};
    UInt64 journals_closed{0};
    UInt64 records_written{0};
    UInt64 bytes_written{0};
    UInt64 flushes{0};
    UInt64 errors{0};
};

// =============================================================================
// Journal Manager
// =============================================================================

class JournalManager {
public:
    static JournalManager& instance();
    
    // Lifecycle
    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }
    
    // Configuration
    void set_journal_directory(StringView dir);
    [[nodiscard]] const String& journal_directory() const { return journal_directory_; }
    
    // Journal operations
    Result<Journal*> open_journal(StringView name, UInt32 number = 0);
    Result<Journal*> get_journal(StringView name);
    Result<Journal*> get_journal(UInt32 number);
    Result<void> close_journal(StringView name);
    
    // Write operations
    Result<UInt64> write(StringView journal_name, StringView jtypeid, 
                         const void* data, UInt32 length);
    Result<UInt64> write(UInt32 journal_number, StringView jtypeid,
                         const void* data, UInt32 length);
    Result<UInt64> write(StringView journal_name, const JournalRecord& record);
    
    // System log shortcut
    Result<UInt64> log(StringView message);
    Result<UInt64> log(StringView jtypeid, StringView message);
    
    // Query
    [[nodiscard]] std::vector<String> list_journals() const;
    [[nodiscard]] std::vector<JournalInfo> list_journal_info() const;
    
    // Context
    void set_current_transaction(StringView transid) { current_transid_ = String(transid); }
    void set_current_task(UInt32 task_id) { current_task_id_ = task_id; }
    
    // Statistics
    [[nodiscard]] JournalStats get_stats() const;
    void reset_stats();
    
private:
    JournalManager() = default;
    ~JournalManager() = default;
    JournalManager(const JournalManager&) = delete;
    JournalManager& operator=(const JournalManager&) = delete;
    
    String generate_filename(StringView name);
    
    bool initialized_ = false;
    String journal_directory_ = "/tmp/cics_journals";
    
    std::unordered_map<String, std::unique_ptr<Journal>> journals_by_name_;
    std::unordered_map<UInt32, Journal*> journals_by_number_;
    
    String current_transid_;
    UInt32 current_task_id_ = 0;
    
    JournalStats stats_;
    mutable std::mutex mutex_;
};

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

// WRITE JOURNALNAME
Result<UInt64> exec_cics_write_journalname(StringView journal_name, 
                                            StringView jtypeid,
                                            const void* data, UInt32 length);
Result<UInt64> exec_cics_write_journalname(StringView journal_name,
                                            const ByteBuffer& data);

// WRITE JOURNALNUM
Result<UInt64> exec_cics_write_journalnum(UInt32 journal_number,
                                           StringView jtypeid,
                                           const void* data, UInt32 length);

// Convenience functions
Result<UInt64> exec_cics_log(StringView message);
Result<UInt64> exec_cics_log(StringView jtypeid, StringView message);

// =============================================================================
// Utility Functions
// =============================================================================

[[nodiscard]] String journal_type_to_string(JournalType type);
[[nodiscard]] String journal_status_to_string(JournalStatus status);

} // namespace journal
} // namespace cics

#endif // CICS_JOURNAL_HPP
