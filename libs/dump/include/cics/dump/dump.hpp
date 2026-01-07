// =============================================================================
// CICS Emulation - Dump Utilities Module
// Version: 3.4.6
// =============================================================================
// Provides hex dump and storage dump formatting utilities for debugging
// and diagnostic purposes.
// =============================================================================

#ifndef CICS_DUMP_HPP
#define CICS_DUMP_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <ostream>
#include <fstream>

namespace cics {
namespace dump {

// =============================================================================
// Dump Options
// =============================================================================

struct DumpOptions {
    UInt32 bytes_per_line = 16;
    bool show_offset = true;
    bool show_hex = true;
    bool show_ascii = true;
    bool show_ebcdic = false;
    bool uppercase_hex = true;
    String offset_format = "%08X";
    Byte unprintable_char = '.';
    bool group_bytes = true;  // Group into 4-byte words
    UInt32 start_offset = 0;  // Starting offset for display
};

// =============================================================================
// Dump Formatting Functions
// =============================================================================

// Format a hex dump to string
[[nodiscard]] String hex_dump(const void* data, UInt32 length);
[[nodiscard]] String hex_dump(const void* data, UInt32 length, const DumpOptions& options);
[[nodiscard]] String hex_dump(const ByteBuffer& data);
[[nodiscard]] String hex_dump(const ByteBuffer& data, const DumpOptions& options);

// Write hex dump to stream
void hex_dump(std::ostream& os, const void* data, UInt32 length);
void hex_dump(std::ostream& os, const void* data, UInt32 length, const DumpOptions& options);

// Write hex dump to file
Result<void> hex_dump_to_file(const String& filename, const void* data, UInt32 length);
Result<void> hex_dump_to_file(const String& filename, const void* data, UInt32 length,
                               const DumpOptions& options);

// =============================================================================
// Storage Dump (CICS-style)
// =============================================================================

struct StorageDumpHeader {
    String title;
    String timestamp;
    String transaction_id;
    String task_number;
    String program_name;
    UInt64 address = 0;
    UInt32 length = 0;
};

// Create CICS-style storage dump
[[nodiscard]] String storage_dump(const void* data, UInt32 length, 
                                   const StorageDumpHeader& header);
[[nodiscard]] String storage_dump(const void* data, UInt32 length);

// Write storage dump to file
Result<void> storage_dump_to_file(const String& filename, const void* data, 
                                   UInt32 length, const StorageDumpHeader& header);

// =============================================================================
// Comparison Dump
// =============================================================================

// Compare two buffers and show differences
[[nodiscard]] String compare_dump(const void* data1, UInt32 length1,
                                   const void* data2, UInt32 length2);
[[nodiscard]] String compare_dump(const ByteBuffer& data1, const ByteBuffer& data2);

// =============================================================================
// Specialized Dumps
// =============================================================================

// Record dump (shows record boundaries)
[[nodiscard]] String record_dump(const void* data, UInt32 length, UInt32 record_length);

// Field dump (shows fields with names and values)
struct FieldInfo {
    String name;
    UInt32 offset;
    UInt32 length;
    String type;  // "CHAR", "PACKED", "BINARY", "HEX"
};

[[nodiscard]] String field_dump(const void* data, UInt32 length, 
                                 const std::vector<FieldInfo>& fields);

// =============================================================================
// Utility Functions
// =============================================================================

// Convert byte to hex string
[[nodiscard]] String byte_to_hex(Byte b, bool uppercase = true);

// Convert bytes to hex string
[[nodiscard]] String bytes_to_hex(const void* data, UInt32 length, bool uppercase = true);
[[nodiscard]] String bytes_to_hex(const ByteBuffer& data, bool uppercase = true);

// Convert hex string to bytes
[[nodiscard]] Result<ByteBuffer> hex_to_bytes(StringView hex_string);

// Format address
[[nodiscard]] String format_address(UInt64 address, UInt8 width = 8);

// Check if byte is printable ASCII
[[nodiscard]] bool is_printable_ascii(Byte b);

// Check if byte is printable EBCDIC
[[nodiscard]] bool is_printable_ebcdic(Byte b);

// Get printable character representation
[[nodiscard]] char get_printable_char(Byte b, Byte replacement = '.');

// =============================================================================
// Dump Statistics
// =============================================================================

struct DumpStats {
    UInt32 total_bytes = 0;
    UInt32 printable_bytes = 0;
    UInt32 zero_bytes = 0;
    UInt32 high_bytes = 0;  // >= 0x80
    std::array<UInt32, 256> byte_histogram = {};
    
    void analyze(const void* data, UInt32 length);
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// Dump File Writer
// =============================================================================

class DumpWriter {
private:
    std::ofstream file_;
    DumpOptions options_;
    UInt64 total_bytes_ = 0;
    
public:
    explicit DumpWriter(const String& filename);
    DumpWriter(const String& filename, const DumpOptions& options);
    ~DumpWriter();
    
    Result<void> open(const String& filename);
    void close();
    [[nodiscard]] bool is_open() const;
    
    Result<void> write_header(const String& title);
    Result<void> write_section(const String& section_name);
    Result<void> write_dump(const void* data, UInt32 length);
    Result<void> write_dump(const void* data, UInt32 length, const String& label);
    Result<void> write_separator();
    Result<void> write_footer();
    
    [[nodiscard]] UInt64 total_bytes() const { return total_bytes_; }
};

// =============================================================================
// Interactive Dump Browser (for debugging)
// =============================================================================

class DumpBrowser {
private:
    const Byte* data_;
    UInt32 length_;
    UInt32 current_offset_ = 0;
    UInt32 page_size_ = 256;
    DumpOptions options_;
    
public:
    DumpBrowser(const void* data, UInt32 length);
    
    void set_page_size(UInt32 size);
    void set_options(const DumpOptions& options);
    
    [[nodiscard]] String current_page() const;
    [[nodiscard]] String page_at(UInt32 offset) const;
    
    void next_page();
    void prev_page();
    void goto_offset(UInt32 offset);
    void goto_start();
    void goto_end();
    
    [[nodiscard]] UInt32 current_offset() const { return current_offset_; }
    [[nodiscard]] UInt32 length() const { return length_; }
    [[nodiscard]] UInt32 page_size() const { return page_size_; }
    
    // Search for pattern
    [[nodiscard]] Result<UInt32> find(const ByteBuffer& pattern, UInt32 start = 0) const;
    [[nodiscard]] Result<UInt32> find_hex(StringView hex_pattern, UInt32 start = 0) const;
    [[nodiscard]] Result<UInt32> find_text(StringView text, UInt32 start = 0) const;
};

} // namespace dump
} // namespace cics

#endif // CICS_DUMP_HPP
