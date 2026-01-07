// =============================================================================
// CICS Emulation - Spool Control
// =============================================================================
// Provides SPOOLOPEN, SPOOLREAD, SPOOLWRITE, SPOOLCLOSE functionality.
// Implements JES spool file handling for report generation and batch output.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#ifndef CICS_SPOOL_HPP
#define CICS_SPOOL_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <fstream>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

namespace cics {
namespace spool {

// =============================================================================
// Constants
// =============================================================================

constexpr UInt32 MAX_RECORD_LENGTH = 32767;
constexpr UInt32 MAX_SPOOL_NAME_LENGTH = 8;
constexpr UInt32 DEFAULT_RECORD_LENGTH = 80;

// =============================================================================
// Enumerations
// =============================================================================

enum class SpoolType {
    INPUT,          // Input spool (SYSIN)
    OUTPUT          // Output spool (SYSOUT)
};

enum class SpoolClass {
    A = 'A',        // Class A
    B = 'B',        // Class B
    C = 'C',        // Class C
    D = 'D',        // Class D
    E = 'E',        // Class E
    F = 'F',        // Class F
    G = 'G',        // Class G
    H = 'H',        // Class H
    I = 'I',        // Class I
    J = 'J',        // Class J
    K = 'K',        // Class K
    L = 'L',        // Class L
    M = 'M',        // Class M
    N = 'N',        // Class N
    O = 'O',        // Class O
    P = 'P',        // Class P (Print)
    Q = 'Q',        // Class Q
    R = 'R',        // Class R
    S = 'S',        // Class S
    T = 'T',        // Class T
    U = 'U',        // Class U
    V = 'V',        // Class V
    W = 'W',        // Class W
    X = 'X',        // Class X
    Y = 'Y',        // Class Y
    Z = 'Z',        // Class Z (Punch)
    STAR = '*'      // Default class
};

enum class SpoolDisposition {
    KEEP,           // Keep after close
    DELETE,         // Delete after close
    HOLD,           // Hold for later release
    RELEASE         // Release for output
};

// =============================================================================
// Spool Attributes
// =============================================================================

struct SpoolAttributes {
    String name;                        // Spool file name (1-8 chars)
    SpoolType type = SpoolType::OUTPUT;
    SpoolClass spool_class = SpoolClass::A;
    SpoolDisposition disposition = SpoolDisposition::KEEP;
    UInt32 record_length = DEFAULT_RECORD_LENGTH;
    UInt32 copies = 1;
    String destination;                 // Output destination
    String form_name;                   // Form name
    String user_data;                   // User data
    bool line_numbers = false;          // Include line numbers
    bool page_numbers = false;          // Include page numbers
    UInt32 lines_per_page = 60;         // Lines per page
};

// =============================================================================
// Spool Information
// =============================================================================

struct SpoolInfo {
    String token;                       // Spool token
    String name;
    SpoolType type;
    SpoolClass spool_class;
    UInt64 record_count;
    UInt64 byte_count;
    UInt32 page_count;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point modified;
    bool is_open;
};

// =============================================================================
// Spool File Class
// =============================================================================

class SpoolFile {
public:
    SpoolFile(StringView token, const SpoolAttributes& attrs);
    ~SpoolFile();
    
    // Properties
    [[nodiscard]] const String& token() const { return token_; }
    [[nodiscard]] const String& name() const { return attrs_.name; }
    [[nodiscard]] SpoolType type() const { return attrs_.type; }
    [[nodiscard]] SpoolClass spool_class() const { return attrs_.spool_class; }
    [[nodiscard]] bool is_open() const { return is_open_; }
    
    // File operations
    Result<void> open();
    Result<void> close();
    
    // Write operations (for OUTPUT spool)
    Result<void> write(StringView data);
    Result<void> write(const ByteBuffer& data);
    Result<void> write_line(StringView line);
    Result<void> new_page();
    
    // Read operations (for INPUT spool)
    Result<ByteBuffer> read();
    Result<String> read_line();
    [[nodiscard]] bool eof() const;
    
    // Position operations
    Result<void> rewind();
    Result<void> skip(UInt32 records);
    
    // Information
    [[nodiscard]] SpoolInfo get_info() const;
    [[nodiscard]] UInt64 record_count() const { return record_count_; }
    [[nodiscard]] UInt64 byte_count() const { return byte_count_; }
    [[nodiscard]] UInt32 current_line() const { return current_line_; }
    [[nodiscard]] UInt32 current_page() const { return current_page_; }
    
private:
    void write_page_header();
    void check_page_break();
    
    String token_;
    SpoolAttributes attrs_;
    bool is_open_ = false;
    
    // File stream
    std::fstream file_;
    String file_path_;
    
    // Statistics
    UInt64 record_count_ = 0;
    UInt64 byte_count_ = 0;
    UInt32 current_line_ = 0;
    UInt32 current_page_ = 0;
    
    mutable std::mutex mutex_;
};

// =============================================================================
// Spool Statistics
// =============================================================================

struct SpoolStats {
    UInt64 files_opened{0};
    UInt64 files_closed{0};
    UInt64 records_written{0};
    UInt64 records_read{0};
    UInt64 bytes_written{0};
    UInt64 bytes_read{0};
    UInt64 pages_output{0};
};

// =============================================================================
// Spool Manager
// =============================================================================

class SpoolManager {
public:
    static SpoolManager& instance();
    
    // Lifecycle
    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }
    
    // Spool directory
    void set_spool_directory(StringView dir);
    [[nodiscard]] const String& spool_directory() const { return spool_directory_; }
    
    // File operations
    Result<String> open(const SpoolAttributes& attrs);  // Returns token
    Result<void> close(StringView token);
    Result<void> close(StringView token, SpoolDisposition disp);
    
    // Write operations
    Result<void> write(StringView token, StringView data);
    Result<void> write(StringView token, const ByteBuffer& data);
    Result<void> write_line(StringView token, StringView line);
    
    // Read operations
    Result<ByteBuffer> read(StringView token);
    Result<String> read_line(StringView token);
    
    // Control operations
    Result<void> new_page(StringView token);
    Result<void> rewind(StringView token);
    
    // Query operations
    Result<SpoolInfo> get_info(StringView token);
    [[nodiscard]] std::vector<SpoolInfo> list_spools() const;
    
    // Statistics
    [[nodiscard]] SpoolStats get_stats() const;
    void reset_stats();
    
private:
    SpoolManager() = default;
    ~SpoolManager() = default;
    SpoolManager(const SpoolManager&) = delete;
    SpoolManager& operator=(const SpoolManager&) = delete;
    
    String generate_token();
    SpoolFile* get_file(StringView token);
    
    bool initialized_ = false;
    String spool_directory_ = "/tmp/cics_spool";
    std::unordered_map<String, std::unique_ptr<SpoolFile>> files_;
    std::atomic<UInt64> token_counter_{0};
    SpoolStats stats_;
    mutable std::mutex mutex_;
};

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

// SPOOLOPEN
Result<String> exec_cics_spoolopen_output(StringView name);
Result<String> exec_cics_spoolopen_output(StringView name, SpoolClass spool_class);
Result<String> exec_cics_spoolopen_output(const SpoolAttributes& attrs);
Result<String> exec_cics_spoolopen_input(StringView name);

// SPOOLWRITE
Result<void> exec_cics_spoolwrite(StringView token, StringView data);
Result<void> exec_cics_spoolwrite(StringView token, const ByteBuffer& data);
Result<void> exec_cics_spoolwrite_line(StringView token, StringView line);

// SPOOLREAD
Result<ByteBuffer> exec_cics_spoolread(StringView token);
Result<String> exec_cics_spoolread_line(StringView token);

// SPOOLCLOSE
Result<void> exec_cics_spoolclose(StringView token);
Result<void> exec_cics_spoolclose(StringView token, SpoolDisposition disp);

// =============================================================================
// Utility Functions
// =============================================================================

[[nodiscard]] String spool_type_to_string(SpoolType type);
[[nodiscard]] char spool_class_to_char(SpoolClass cls);
[[nodiscard]] SpoolClass char_to_spool_class(char c);
[[nodiscard]] String spool_disposition_to_string(SpoolDisposition disp);

} // namespace spool
} // namespace cics

#endif // CICS_SPOOL_HPP
