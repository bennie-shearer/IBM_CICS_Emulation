// =============================================================================
// CICS Emulation - Copybook Parser Module
// Version: 3.4.6
// =============================================================================
// Parses COBOL copybooks and generates C++ structures for record layouts.
// Supports PIC clauses, OCCURS, REDEFINES, and common COBOL data types.
// =============================================================================

#ifndef CICS_COPYBOOK_HPP
#define CICS_COPYBOOK_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <vector>
#include <memory>
#include <unordered_map>

namespace cics {
namespace copybook {

// =============================================================================
// COBOL Data Types
// =============================================================================

enum class DataType : UInt8 {
    ALPHANUMERIC,     // PIC X(n)
    ALPHABETIC,       // PIC A(n)
    NUMERIC_DISPLAY,  // PIC 9(n) or PIC S9(n)
    NUMERIC_PACKED,   // PIC S9(n) COMP-3
    NUMERIC_BINARY,   // PIC S9(n) COMP or BINARY
    NUMERIC_FLOAT,    // COMP-1 (float) or COMP-2 (double)
    GROUP,            // Group item (contains other items)
    FILLER            // Unnamed field
};

enum class UsageClause : UInt8 {
    DISPLAY,    // Default
    COMP,       // Binary (COMP, COMP-4, BINARY)
    COMP_1,     // Single-precision float
    COMP_2,     // Double-precision float
    COMP_3,     // Packed decimal
    COMP_5,     // Native binary
    POINTER,    // Pointer
    INDEX       // Index
};

// =============================================================================
// Picture Clause Information
// =============================================================================

struct PictureClause {
    String raw_picture;       // Original PIC string
    DataType type = DataType::ALPHANUMERIC;
    UInt16 total_digits = 0;  // Total size in bytes/digits
    UInt16 decimal_digits = 0; // Digits after decimal point
    bool is_signed = false;
    bool has_decimal = false;
    char sign_position = ' '; // 'L' leading, 'T' trailing, 'S' separate
    
    [[nodiscard]] UInt16 storage_size() const;
    [[nodiscard]] String to_cpp_type() const;
    [[nodiscard]] String to_string() const;
    
    static Result<PictureClause> parse(StringView pic);
};

// =============================================================================
// Field Definition
// =============================================================================

struct CopybookField {
    UInt8 level = 0;              // 01-49, 66, 77, 88
    String name;
    PictureClause picture;
    UsageClause usage = UsageClause::DISPLAY;
    UInt16 occurs = 0;            // OCCURS count (0 = not array)
    UInt16 occurs_min = 0;        // OCCURS DEPENDING ON min
    String occurs_depending;       // OCCURS DEPENDING ON field name
    String redefines;             // REDEFINES field name
    String value;                 // VALUE clause
    UInt32 offset = 0;            // Byte offset in record
    UInt16 size = 0;              // Total size in bytes
    std::vector<std::unique_ptr<CopybookField>> children;  // For group items
    CopybookField* parent = nullptr;
    
    [[nodiscard]] bool is_group() const { return !children.empty(); }
    [[nodiscard]] bool is_elementary() const { return children.empty() && level != 88; }
    [[nodiscard]] bool is_condition() const { return level == 88; }
    [[nodiscard]] bool is_array() const { return occurs > 0; }
    [[nodiscard]] UInt32 total_size() const;
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_cpp_declaration() const;
};

// =============================================================================
// Copybook Definition
// =============================================================================

struct CopybookDefinition {
    String name;
    String source_file;
    std::vector<std::unique_ptr<CopybookField>> fields;
    UInt32 record_length = 0;
    
    [[nodiscard]] const CopybookField* find_field(StringView name) const;
    [[nodiscard]] std::vector<const CopybookField*> get_all_fields() const;
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_cpp_struct() const;
    [[nodiscard]] String to_cpp_header() const;
};

// =============================================================================
// Copybook Parser
// =============================================================================

class CopybookParser {
private:
    String source_;
    size_t position_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;
    std::vector<String> errors_;
    
    // Parsing helpers
    void skip_whitespace();
    void skip_to_next_line();
    bool at_end() const;
    char peek() const;
    char advance();
    bool match(char c);
    bool match(StringView str);
    String read_word();
    String read_until(char delimiter);
    String read_picture();
    
    // Parse components
    Result<std::unique_ptr<CopybookField>> parse_field();
    Result<UInt8> parse_level();
    Result<String> parse_name();
    Result<PictureClause> parse_picture_clause();
    Result<UsageClause> parse_usage_clause();
    Result<UInt16> parse_occurs();
    Result<String> parse_redefines();
    Result<String> parse_value();
    
    void calculate_offsets(CopybookDefinition& copybook);
    
public:
    CopybookParser() = default;
    
    Result<CopybookDefinition> parse(StringView source);
    Result<CopybookDefinition> parse_file(StringView filename);
    
    [[nodiscard]] const std::vector<String>& errors() const { return errors_; }
};

// =============================================================================
// C++ Code Generator
// =============================================================================

class CppGenerator {
private:
    String namespace_name_;
    bool use_pragma_pack_ = true;
    bool generate_accessors_ = true;
    bool generate_serialize_ = true;
    
public:
    CppGenerator() = default;
    
    void set_namespace(StringView ns) { namespace_name_ = String(ns); }
    void set_pragma_pack(bool enable) { use_pragma_pack_ = enable; }
    void set_generate_accessors(bool enable) { generate_accessors_ = enable; }
    void set_generate_serialize(bool enable) { generate_serialize_ = enable; }
    
    [[nodiscard]] String generate_header(const CopybookDefinition& copybook) const;
    [[nodiscard]] String generate_source(const CopybookDefinition& copybook) const;
    
    Result<void> write_header(const CopybookDefinition& copybook, StringView path) const;
    Result<void> write_source(const CopybookDefinition& copybook, StringView path) const;
};

// =============================================================================
// Record Accessor
// =============================================================================

class RecordAccessor {
private:
    const CopybookDefinition& copybook_;
    Byte* data_;
    UInt32 length_;
    
public:
    RecordAccessor(const CopybookDefinition& copybook, void* data, UInt32 length);
    
    // Field access
    [[nodiscard]] Result<String> get_string(StringView field_name) const;
    [[nodiscard]] Result<Int64> get_integer(StringView field_name) const;
    [[nodiscard]] Result<double> get_decimal(StringView field_name) const;
    [[nodiscard]] Result<ByteBuffer> get_raw(StringView field_name) const;
    
    Result<void> set_string(StringView field_name, StringView value);
    Result<void> set_integer(StringView field_name, Int64 value);
    Result<void> set_decimal(StringView field_name, double value);
    Result<void> set_raw(StringView field_name, const ByteBuffer& value);
    
    // Array access
    [[nodiscard]] Result<String> get_string(StringView field_name, UInt16 index) const;
    Result<void> set_string(StringView field_name, UInt16 index, StringView value);
    
    // Utility
    void clear();
    [[nodiscard]] String dump() const;
};

// =============================================================================
// Utility Functions
// =============================================================================

// Convert COBOL name to C++ identifier
[[nodiscard]] String cobol_to_cpp_name(StringView cobol_name);

// Calculate storage size for COMP fields
[[nodiscard]] UInt16 comp_storage_size(UInt16 digits);

// Parse PIC repetition (e.g., "X(10)" -> 10)
[[nodiscard]] UInt16 parse_pic_count(StringView pic);

} // namespace copybook
} // namespace cics

#endif // CICS_COPYBOOK_HPP
