// =============================================================================
// CICS Emulation - Dump Utilities Module Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/dump/dump.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <cstring>

namespace cics {
namespace dump {

// =============================================================================
// Utility Functions Implementation
// =============================================================================

String byte_to_hex(Byte b, bool uppercase) {
    static const char hex_lower[] = "0123456789abcdef";
    static const char hex_upper[] = "0123456789ABCDEF";
    const char* hex = uppercase ? hex_upper : hex_lower;
    
    char result[3];
    result[0] = hex[(b >> 4) & 0x0F];
    result[1] = hex[b & 0x0F];
    result[2] = '\0';
    return result;
}

String bytes_to_hex(const void* data, UInt32 length, bool uppercase) {
    if (!data || length == 0) return "";
    
    String result;
    result.reserve(length * 2);
    
    const Byte* bytes = static_cast<const Byte*>(data);
    for (UInt32 i = 0; i < length; ++i) {
        result += byte_to_hex(bytes[i], uppercase);
    }
    
    return result;
}

String bytes_to_hex(const ByteBuffer& data, bool uppercase) {
    return bytes_to_hex(data.data(), static_cast<UInt32>(data.size()), uppercase);
}

Result<ByteBuffer> hex_to_bytes(StringView hex_string) {
    String hex(hex_string);
    
    // Remove spaces and common separators
    hex.erase(std::remove_if(hex.begin(), hex.end(), 
        [](char c) { return c == ' ' || c == ':' || c == '-'; }), hex.end());
    
    if (hex.length() % 2 != 0) {
        return make_error<ByteBuffer>(ErrorCode::INVALID_ARGUMENT,
            "Hex string must have even length");
    }
    
    ByteBuffer result;
    result.reserve(hex.length() / 2);
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        char high = hex[i];
        char low = hex[i + 1];
        
        auto hex_value = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        
        int h = hex_value(high);
        int l = hex_value(low);
        
        if (h < 0 || l < 0) {
            return make_error<ByteBuffer>(ErrorCode::INVALID_ARGUMENT,
                "Invalid hex character");
        }
        
        result.push_back(static_cast<Byte>((h << 4) | l));
    }
    
    return make_success(std::move(result));
}

String format_address(UInt64 address, UInt8 width) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(width) << std::uppercase << std::hex << address;
    return oss.str();
}

bool is_printable_ascii(Byte b) {
    return b >= 0x20 && b <= 0x7E;
}

bool is_printable_ebcdic(Byte b) {
    return b >= 0x40 && b <= 0xFE;
}

char get_printable_char(Byte b, Byte replacement) {
    return is_printable_ascii(b) ? static_cast<char>(b) : static_cast<char>(replacement);
}

// =============================================================================
// Hex Dump Implementation
// =============================================================================

String hex_dump(const void* data, UInt32 length) {
    DumpOptions options;
    return hex_dump(data, length, options);
}

String hex_dump(const void* data, UInt32 length, const DumpOptions& options) {
    if (!data || length == 0) return "";
    
    std::ostringstream oss;
    hex_dump(oss, data, length, options);
    return oss.str();
}

String hex_dump(const ByteBuffer& data) {
    return hex_dump(data.data(), static_cast<UInt32>(data.size()));
}

String hex_dump(const ByteBuffer& data, const DumpOptions& options) {
    return hex_dump(data.data(), static_cast<UInt32>(data.size()), options);
}

void hex_dump(std::ostream& os, const void* data, UInt32 length) {
    DumpOptions options;
    hex_dump(os, data, length, options);
}

void hex_dump(std::ostream& os, const void* data, UInt32 length, const DumpOptions& options) {
    if (!data || length == 0) return;
    
    const Byte* bytes = static_cast<const Byte*>(data);
    const char* hex_chars = options.uppercase_hex ? "0123456789ABCDEF" : "0123456789abcdef";
    
    for (UInt32 offset = 0; offset < length; offset += options.bytes_per_line) {
        // Offset
        if (options.show_offset) {
            os << std::setfill('0') << std::setw(8) << std::uppercase << std::hex 
               << (options.start_offset + offset) << "  ";
        }
        
        // Hex values
        if (options.show_hex) {
            for (UInt32 i = 0; i < options.bytes_per_line; ++i) {
                if (offset + i < length) {
                    Byte b = bytes[offset + i];
                    os << hex_chars[(b >> 4) & 0x0F] << hex_chars[b & 0x0F];
                } else {
                    os << "  ";
                }
                
                if (options.group_bytes && (i + 1) % 4 == 0) {
                    os << ' ';
                } else if (!options.group_bytes) {
                    os << ' ';
                }
            }
            os << ' ';
        }
        
        // ASCII representation
        if (options.show_ascii) {
            os << '|';
            for (UInt32 i = 0; i < options.bytes_per_line; ++i) {
                if (offset + i < length) {
                    Byte b = bytes[offset + i];
                    os << get_printable_char(b, options.unprintable_char);
                } else {
                    os << ' ';
                }
            }
            os << '|';
        }
        
        os << '\n';
    }
}

Result<void> hex_dump_to_file(const String& filename, const void* data, UInt32 length) {
    DumpOptions options;
    return hex_dump_to_file(filename, data, length, options);
}

Result<void> hex_dump_to_file(const String& filename, const void* data, UInt32 length,
                               const DumpOptions& options) {
    std::ofstream file(filename);
    if (!file) {
        return make_error<void>(ErrorCode::IO_ERROR, "Failed to open file: " + filename);
    }
    
    hex_dump(file, data, length, options);
    return make_success();
}

// =============================================================================
// Storage Dump Implementation
// =============================================================================

String storage_dump(const void* data, UInt32 length) {
    StorageDumpHeader header;
    header.title = "STORAGE DUMP";
    
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&time_t_now);
    
    std::ostringstream ts;
    ts << std::put_time(tm_now, "%Y-%m-%d %H:%M:%S");
    header.timestamp = ts.str();
    header.address = reinterpret_cast<UInt64>(data);
    header.length = length;
    
    return storage_dump(data, length, header);
}

String storage_dump(const void* data, UInt32 length, const StorageDumpHeader& header) {
    std::ostringstream oss;
    
    // Header
    oss << std::string(78, '=') << '\n';
    oss << header.title << '\n';
    oss << std::string(78, '=') << '\n';
    
    if (!header.timestamp.empty()) {
        oss << "Timestamp:      " << header.timestamp << '\n';
    }
    if (!header.transaction_id.empty()) {
        oss << "Transaction:    " << header.transaction_id << '\n';
    }
    if (!header.task_number.empty()) {
        oss << "Task:           " << header.task_number << '\n';
    }
    if (!header.program_name.empty()) {
        oss << "Program:        " << header.program_name << '\n';
    }
    
    oss << "Address:        " << format_address(header.address, 16) << '\n';
    oss << "Length:         " << std::dec << header.length << " bytes (0x" 
        << std::hex << header.length << ")\n";
    oss << std::string(78, '-') << '\n';
    
    // Dump content
    DumpOptions options;
    options.bytes_per_line = 16;
    options.start_offset = static_cast<UInt32>(header.address & 0xFFFFFFFF);
    hex_dump(oss, data, length, options);
    
    oss << std::string(78, '=') << '\n';
    
    return oss.str();
}

Result<void> storage_dump_to_file(const String& filename, const void* data,
                                   UInt32 length, const StorageDumpHeader& header) {
    std::ofstream file(filename);
    if (!file) {
        return make_error<void>(ErrorCode::IO_ERROR, "Failed to open file: " + filename);
    }
    
    file << storage_dump(data, length, header);
    return make_success();
}

// =============================================================================
// Comparison Dump Implementation
// =============================================================================

String compare_dump(const void* data1, UInt32 length1,
                     const void* data2, UInt32 length2) {
    std::ostringstream oss;
    
    const Byte* bytes1 = static_cast<const Byte*>(data1);
    const Byte* bytes2 = static_cast<const Byte*>(data2);
    UInt32 max_length = std::max(length1, length2);
    UInt32 diff_count = 0;
    
    oss << "Comparison Dump\n";
    oss << std::string(78, '=') << '\n';
    oss << "Buffer 1: " << length1 << " bytes\n";
    oss << "Buffer 2: " << length2 << " bytes\n";
    oss << std::string(78, '-') << '\n';
    oss << "Offset    Buffer 1   Buffer 2   Diff\n";
    oss << std::string(78, '-') << '\n';
    
    for (UInt32 i = 0; i < max_length; ++i) {
        Byte b1 = (i < length1) ? bytes1[i] : 0;
        Byte b2 = (i < length2) ? bytes2[i] : 0;
        
        if (b1 != b2 || i >= length1 || i >= length2) {
            oss << std::setfill('0') << std::setw(8) << std::hex << i << "  ";
            
            if (i < length1) {
                oss << byte_to_hex(b1) << " '" << get_printable_char(b1) << "'";
            } else {
                oss << "-- '-'";
            }
            
            oss << "    ";
            
            if (i < length2) {
                oss << byte_to_hex(b2) << " '" << get_printable_char(b2) << "'";
            } else {
                oss << "-- '-'";
            }
            
            oss << "    *\n";
            ++diff_count;
        }
    }
    
    oss << std::string(78, '-') << '\n';
    oss << std::dec << "Total differences: " << diff_count << '\n';
    
    return oss.str();
}

String compare_dump(const ByteBuffer& data1, const ByteBuffer& data2) {
    return compare_dump(data1.data(), static_cast<UInt32>(data1.size()),
                        data2.data(), static_cast<UInt32>(data2.size()));
}

// =============================================================================
// Record Dump Implementation
// =============================================================================

String record_dump(const void* data, UInt32 length, UInt32 record_length) {
    if (record_length == 0) record_length = 80;  // Default COBOL record
    
    std::ostringstream oss;
    const Byte* bytes = static_cast<const Byte*>(data);
    UInt32 record_num = 0;
    
    oss << "Record Dump (record length=" << record_length << ")\n";
    oss << std::string(78, '=') << '\n';
    
    for (UInt32 offset = 0; offset < length; offset += record_length) {
        UInt32 rec_len = std::min(record_length, length - offset);
        
        oss << "Record " << std::setw(6) << std::dec << record_num++ 
            << " (offset " << std::setfill('0') << std::setw(8) << std::hex << offset << "):\n";
        
        DumpOptions options;
        options.bytes_per_line = 16;
        hex_dump(oss, bytes + offset, rec_len, options);
        oss << '\n';
    }
    
    return oss.str();
}

// =============================================================================
// Field Dump Implementation
// =============================================================================

String field_dump(const void* data, UInt32 length,
                   const std::vector<FieldInfo>& fields) {
    std::ostringstream oss;
    const Byte* bytes = static_cast<const Byte*>(data);
    
    oss << "Field Dump\n";
    oss << std::string(78, '=') << '\n';
    oss << std::left << std::setw(20) << "Field Name"
        << std::setw(8) << "Offset"
        << std::setw(6) << "Len"
        << std::setw(8) << "Type"
        << "Value\n";
    oss << std::string(78, '-') << '\n';
    
    for (const auto& field : fields) {
        if (field.offset >= length) continue;
        
        UInt32 field_len = std::min(field.length, length - field.offset);
        
        oss << std::left << std::setw(20) << field.name
            << std::setfill('0') << std::setw(8) << std::hex << field.offset
            << std::setfill(' ') << std::setw(6) << std::dec << field.length
            << std::setw(8) << field.type;
        
        // Format value based on type
        if (field.type == "CHAR" || field.type == "PIC X") {
            String value;
            for (UInt32 i = 0; i < field_len; ++i) {
                value += get_printable_char(bytes[field.offset + i]);
            }
            oss << "'" << value << "'";
        } else if (field.type == "HEX") {
            oss << bytes_to_hex(bytes + field.offset, field_len);
        } else if (field.type == "PACKED" || field.type == "COMP-3") {
            oss << bytes_to_hex(bytes + field.offset, field_len) << " (packed)";
        } else if (field.type == "BINARY" || field.type == "COMP") {
            oss << bytes_to_hex(bytes + field.offset, field_len) << " (binary)";
        } else {
            oss << bytes_to_hex(bytes + field.offset, field_len);
        }
        
        oss << '\n';
    }
    
    return oss.str();
}

// =============================================================================
// DumpStats Implementation
// =============================================================================

void DumpStats::analyze(const void* data, UInt32 length) {
    if (!data || length == 0) return;
    
    const Byte* bytes = static_cast<const Byte*>(data);
    total_bytes = length;
    
    byte_histogram.fill(0);
    printable_bytes = 0;
    zero_bytes = 0;
    high_bytes = 0;
    
    for (UInt32 i = 0; i < length; ++i) {
        Byte b = bytes[i];
        ++byte_histogram[b];
        
        if (is_printable_ascii(b)) ++printable_bytes;
        if (b == 0) ++zero_bytes;
        if (b >= 0x80) ++high_bytes;
    }
}

String DumpStats::to_string() const {
    std::ostringstream oss;
    
    oss << "Dump Statistics\n";
    oss << std::string(40, '-') << '\n';
    oss << "Total bytes:     " << std::setw(10) << total_bytes << '\n';
    oss << "Printable:       " << std::setw(10) << printable_bytes 
        << " (" << std::setprecision(1) << std::fixed
        << (total_bytes ? 100.0 * printable_bytes / total_bytes : 0) << "%)\n";
    oss << "Zero bytes:      " << std::setw(10) << zero_bytes
        << " (" << (total_bytes ? 100.0 * zero_bytes / total_bytes : 0) << "%)\n";
    oss << "High bytes:      " << std::setw(10) << high_bytes
        << " (" << (total_bytes ? 100.0 * high_bytes / total_bytes : 0) << "%)\n";
    
    // Find most common bytes
    std::vector<std::pair<Byte, UInt32>> sorted_hist;
    for (int i = 0; i < 256; ++i) {
        if (byte_histogram[i] > 0) {
            sorted_hist.emplace_back(static_cast<Byte>(i), byte_histogram[i]);
        }
    }
    std::sort(sorted_hist.begin(), sorted_hist.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    oss << "\nMost common bytes:\n";
    for (size_t i = 0; i < std::min(size_t(10), sorted_hist.size()); ++i) {
        oss << "  0x" << byte_to_hex(sorted_hist[i].first) 
            << " '" << get_printable_char(sorted_hist[i].first) << "'"
            << ": " << sorted_hist[i].second << '\n';
    }
    
    return oss.str();
}

// =============================================================================
// DumpWriter Implementation
// =============================================================================

DumpWriter::DumpWriter(const String& filename) {
    open(filename);
}

DumpWriter::DumpWriter(const String& filename, const DumpOptions& options)
    : options_(options) {
    open(filename);
}

DumpWriter::~DumpWriter() {
    close();
}

Result<void> DumpWriter::open(const String& filename) {
    file_.open(filename);
    if (!file_) {
        return make_error<void>(ErrorCode::IO_ERROR, "Failed to open file: " + filename);
    }
    return make_success();
}

void DumpWriter::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool DumpWriter::is_open() const {
    return file_.is_open();
}

Result<void> DumpWriter::write_header(const String& title) {
    if (!file_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "File not open");
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    file_ << std::string(78, '=') << '\n';
    file_ << title << '\n';
    file_ << "Generated: " << std::ctime(&time_t_now);
    file_ << std::string(78, '=') << '\n';
    
    return make_success();
}

Result<void> DumpWriter::write_section(const String& section_name) {
    if (!file_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "File not open");
    }
    
    file_ << '\n' << std::string(78, '-') << '\n';
    file_ << section_name << '\n';
    file_ << std::string(78, '-') << '\n';
    
    return make_success();
}

Result<void> DumpWriter::write_dump(const void* data, UInt32 length) {
    if (!file_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "File not open");
    }
    
    hex_dump(file_, data, length, options_);
    total_bytes_ += length;
    
    return make_success();
}

Result<void> DumpWriter::write_dump(const void* data, UInt32 length, const String& label) {
    if (!file_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "File not open");
    }
    
    file_ << label << " (" << length << " bytes):\n";
    hex_dump(file_, data, length, options_);
    total_bytes_ += length;
    
    return make_success();
}

Result<void> DumpWriter::write_separator() {
    if (!file_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "File not open");
    }
    
    file_ << std::string(78, '-') << '\n';
    return make_success();
}

Result<void> DumpWriter::write_footer() {
    if (!file_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "File not open");
    }
    
    file_ << std::string(78, '=') << '\n';
    file_ << "Total bytes dumped: " << total_bytes_ << '\n';
    file_ << std::string(78, '=') << '\n';
    
    return make_success();
}

// =============================================================================
// DumpBrowser Implementation
// =============================================================================

DumpBrowser::DumpBrowser(const void* data, UInt32 length)
    : data_(static_cast<const Byte*>(data)), length_(length) {}

void DumpBrowser::set_page_size(UInt32 size) {
    page_size_ = size;
}

void DumpBrowser::set_options(const DumpOptions& options) {
    options_ = options;
}

String DumpBrowser::current_page() const {
    return page_at(current_offset_);
}

String DumpBrowser::page_at(UInt32 offset) const {
    if (offset >= length_) return "";
    
    UInt32 len = std::min(page_size_, length_ - offset);
    DumpOptions opts = options_;
    opts.start_offset = offset;
    
    return hex_dump(data_ + offset, len, opts);
}

void DumpBrowser::next_page() {
    if (current_offset_ + page_size_ < length_) {
        current_offset_ += page_size_;
    }
}

void DumpBrowser::prev_page() {
    if (current_offset_ >= page_size_) {
        current_offset_ -= page_size_;
    } else {
        current_offset_ = 0;
    }
}

void DumpBrowser::goto_offset(UInt32 offset) {
    current_offset_ = std::min(offset, length_ > 0 ? length_ - 1 : 0);
}

void DumpBrowser::goto_start() {
    current_offset_ = 0;
}

void DumpBrowser::goto_end() {
    if (length_ > page_size_) {
        current_offset_ = length_ - page_size_;
    } else {
        current_offset_ = 0;
    }
}

Result<UInt32> DumpBrowser::find(const ByteBuffer& pattern, UInt32 start) const {
    if (pattern.empty() || start >= length_) {
        return make_error<UInt32>(ErrorCode::RECORD_NOT_FOUND, "Pattern not found");
    }
    
    for (UInt32 i = start; i <= length_ - pattern.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (data_[i + j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return make_success(i);
        }
    }
    
    return make_error<UInt32>(ErrorCode::RECORD_NOT_FOUND, "Pattern not found");
}

Result<UInt32> DumpBrowser::find_hex(StringView hex_pattern, UInt32 start) const {
    auto bytes = hex_to_bytes(hex_pattern);
    if (!bytes.is_success()) {
        return make_error<UInt32>(bytes.error().code, bytes.error().message);
    }
    return find(bytes.value(), start);
}

Result<UInt32> DumpBrowser::find_text(StringView text, UInt32 start) const {
    ByteBuffer pattern(text.begin(), text.end());
    return find(pattern, start);
}

} // namespace dump
} // namespace cics
