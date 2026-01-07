// =============================================================================
// CICS Emulation - EBCDIC Conversion Example
// Version: 3.4.6
// =============================================================================
// Demonstrates EBCDIC/ASCII conversion and packed decimal handling
// =============================================================================

#include <cics/ebcdic/ebcdic.hpp>
#include <iostream>
#include <iomanip>

using namespace cics;
using namespace cics::ebcdic;

void print_header(const char* title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void print_hex(const Byte* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec;
}

int main() {
    std::cout << R"(
+==============================================================+
|      CICS Emulation - EBCDIC Conversion Example   |
|                        Version 3.4.6                         |
+==============================================================+
)";

    // =========================================================================
    // 1. ASCII to EBCDIC Conversion
    // =========================================================================
    print_header("1. ASCII to EBCDIC Conversion");
    
    String ascii_text = "Hello, CICS World!";
    std::cout << "  ASCII text: \"" << ascii_text << "\"\n";
    std::cout << "  ASCII hex:  ";
    for (char c : ascii_text) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    std::cout << std::dec << "\n";
    
    ByteBuffer ebcdic_text = string_to_ebcdic(ascii_text);
    std::cout << "  EBCDIC hex: ";
    print_hex(ebcdic_text.data(), ebcdic_text.size());
    std::cout << "\n";

    // =========================================================================
    // 2. EBCDIC to ASCII Conversion
    // =========================================================================
    print_header("2. EBCDIC to ASCII Conversion");
    
    // EBCDIC representation of "MAINFRAME"
    ByteBuffer ebcdic_sample = {0xD4, 0xC1, 0xC9, 0xD5, 0xC6, 0xD9, 0xC1, 0xD4, 0xC5};
    std::cout << "  EBCDIC hex: ";
    print_hex(ebcdic_sample.data(), ebcdic_sample.size());
    std::cout << "\n";
    
    String converted = ebcdic_to_string(ebcdic_sample);
    std::cout << "  ASCII text: \"" << converted << "\"\n";

    // =========================================================================
    // 3. In-Place Conversion
    // =========================================================================
    print_header("3. In-Place Conversion");
    
    ByteBuffer data = {'A', 'B', 'C', '1', '2', '3'};
    std::cout << "  Original (ASCII): ";
    for (Byte b : data) std::cout << static_cast<char>(b);
    std::cout << " (hex: ";
    print_hex(data.data(), data.size());
    std::cout << ")\n";
    
    ascii_to_ebcdic(data.data(), static_cast<UInt32>(data.size()));
    std::cout << "  Converted (EBCDIC hex): ";
    print_hex(data.data(), data.size());
    std::cout << "\n";
    
    ebcdic_to_ascii(data.data(), static_cast<UInt32>(data.size()));
    std::cout << "  Back to ASCII: ";
    for (Byte b : data) std::cout << static_cast<char>(b);
    std::cout << "\n";

    // =========================================================================
    // 4. Packed Decimal (COMP-3) Operations
    // =========================================================================
    print_header("4. Packed Decimal (COMP-3) Operations");
    
    // Create packed decimal from integer
    Int64 value = 12345;
    ByteBuffer packed = int64_to_packed(value, 4);
    std::cout << "  Integer: " << value << "\n";
    std::cout << "  Packed:  ";
    print_hex(packed.data(), packed.size());
    std::cout << "\n";
    
    // Convert back
    Int64 unpacked = packed_to_int64(packed.data(), static_cast<UInt32>(packed.size()));
    std::cout << "  Unpacked: " << unpacked << "\n";
    
    // Negative number
    value = -9876;
    packed = int64_to_packed(value, 4);
    std::cout << "\n  Integer: " << value << "\n";
    std::cout << "  Packed:  ";
    print_hex(packed.data(), packed.size());
    std::cout << "\n";
    std::cout << "  Sign: " << (is_positive_packed(packed.data(), static_cast<UInt32>(packed.size())) ? "+" : "-") << "\n";

    // =========================================================================
    // 5. PackedDecimal Class
    // =========================================================================
    print_header("5. PackedDecimal Class");
    
    // From integer
    cics::ebcdic::PackedDecimal pd1 = cics::ebcdic::PackedDecimal::from_int64(123456789, 10, 2);
    std::cout << "  From int64 (123456789, scale=2):\n";
    std::cout << "    to_int64(): " << pd1.to_int64() << "\n";
    std::cout << "    to_double(): " << pd1.to_double() << "\n";
    std::cout << "    to_display(): " << pd1.to_display() << "\n";
    std::cout << "    hex: ";
    print_hex(pd1.data.data(), pd1.data.size());
    std::cout << "\n";
    
    // From double
    cics::ebcdic::PackedDecimal pd2 = cics::ebcdic::PackedDecimal::from_double(1234.56, 8, 2);
    std::cout << "\n  From double (1234.56):\n";
    std::cout << "    to_display(): " << pd2.to_display() << "\n";
    std::cout << "    hex: ";
    print_hex(pd2.data.data(), pd2.data.size());
    std::cout << "\n";

    // =========================================================================
    // 6. Packed Decimal Validation
    // =========================================================================
    print_header("6. Packed Decimal Validation");
    
    ByteBuffer valid_packed = {0x12, 0x34, 0x5C};  // +12345
    ByteBuffer invalid_packed = {0xAB, 0xCD, 0xEF}; // Invalid digits
    
    std::cout << "  Valid packed (12 34 5C): " 
              << (is_valid_packed(valid_packed.data(), 3) ? "YES" : "NO") << "\n";
    std::cout << "  Invalid packed (AB CD EF): " 
              << (is_valid_packed(invalid_packed.data(), 3) ? "YES" : "NO") << "\n";

    // =========================================================================
    // 7. Zoned Decimal
    // =========================================================================
    print_header("7. Zoned Decimal (DISPLAY NUMERIC)");
    
    value = 12345;
    ByteBuffer zoned = int64_to_zoned(value, 6);
    std::cout << "  Integer: " << value << "\n";
    std::cout << "  Zoned:   ";
    print_hex(zoned.data(), zoned.size());
    std::cout << "\n";
    
    // Convert back
    Int64 unzoned = zoned_to_int64(zoned.data(), static_cast<UInt32>(zoned.size()));
    std::cout << "  Unzoned: " << unzoned << "\n";
    
    // Negative
    value = -9876;
    zoned = int64_to_zoned(value, 6);
    std::cout << "\n  Integer: " << value << "\n";
    std::cout << "  Zoned:   ";
    print_hex(zoned.data(), zoned.size());
    std::cout << " (note: D zone = negative)\n";

    // =========================================================================
    // 8. Binary (COMP) Conversion
    // =========================================================================
    print_header("8. Binary (COMP) Conversion - Big Endian");
    
    Int32 int_value = 0x12345678;
    Byte binary[4];
    int32_to_binary(int_value, binary);
    
    std::cout << "  Integer: 0x" << std::hex << int_value << std::dec << "\n";
    std::cout << "  Binary (big-endian): ";
    print_hex(binary, 4);
    std::cout << "\n";
    
    Int32 recovered = binary_to_int32(binary);
    std::cout << "  Recovered: 0x" << std::hex << recovered << std::dec << "\n";

    // =========================================================================
    // 9. EBCDIC Character Classification
    // =========================================================================
    print_header("9. EBCDIC Character Classification");
    
    std::cout << "  EBCDIC character checks:\n";
    std::cout << "    0xC1 (A): alpha=" << is_ebcdic_alpha(0xC1) 
              << ", digit=" << is_ebcdic_digit(0xC1) << "\n";
    std::cout << "    0xF5 (5): alpha=" << is_ebcdic_alpha(0xF5) 
              << ", digit=" << is_ebcdic_digit(0xF5) << "\n";
    std::cout << "    0x40 (space): space=" << is_ebcdic_space(0x40) 
              << ", printable=" << is_ebcdic_printable(0x40) << "\n";
    std::cout << "    0x00 (null): printable=" << is_ebcdic_printable(0x00) << "\n";

    // =========================================================================
    // 10. Common EBCDIC Constants
    // =========================================================================
    print_header("10. Common EBCDIC Constants");
    
    std::cout << "  EBCDIC_SPACE:  0x" << std::hex << static_cast<int>(EBCDIC_SPACE) << "\n";
    std::cout << "  EBCDIC_ZERO:   0x" << static_cast<int>(EBCDIC_ZERO) << "\n";
    std::cout << "  EBCDIC_NINE:   0x" << static_cast<int>(EBCDIC_NINE) << "\n";
    std::cout << "  EBCDIC_A:      0x" << static_cast<int>(EBCDIC_A) << "\n";
    std::cout << "  EBCDIC_Z:      0x" << static_cast<int>(EBCDIC_Z) << "\n";
    std::cout << std::dec;

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " EBCDIC Conversion Example Complete\n";
    std::cout << std::string(60, '=') << "\n";

    return 0;
}
