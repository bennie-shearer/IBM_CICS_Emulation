// =============================================================================
// CICS Emulation - EBCDIC Conversion Module
// Version: 3.4.6
// =============================================================================
// Provides EBCDIC/ASCII character conversion and packed decimal handling
// essential for mainframe data processing.
// =============================================================================

#ifndef CICS_EBCDIC_HPP
#define CICS_EBCDIC_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <array>

namespace cics {
namespace ebcdic {

// =============================================================================
// Code Pages
// =============================================================================

enum class CodePage : UInt16 {
    IBM037 = 37,    // US/Canada EBCDIC
    IBM273 = 273,   // German EBCDIC
    IBM277 = 277,   // Danish/Norwegian EBCDIC
    IBM278 = 278,   // Finnish/Swedish EBCDIC
    IBM280 = 280,   // Italian EBCDIC
    IBM284 = 284,   // Spanish EBCDIC
    IBM285 = 285,   // UK EBCDIC
    IBM297 = 297,   // French EBCDIC
    IBM500 = 500,   // International EBCDIC
    IBM1047 = 1047, // Latin-1/Open Systems
    ASCII = 0       // Standard ASCII (for reference)
};

// =============================================================================
// Translation Tables
// =============================================================================

// EBCDIC to ASCII translation table (IBM-037)
extern const std::array<Byte, 256> EBCDIC_TO_ASCII;

// ASCII to EBCDIC translation table (IBM-037)
extern const std::array<Byte, 256> ASCII_TO_EBCDIC;

// =============================================================================
// Character Conversion Functions
// =============================================================================

// Single character conversion
[[nodiscard]] Byte ebcdic_to_ascii(Byte ebcdic_char);
[[nodiscard]] Byte ascii_to_ebcdic(Byte ascii_char);

// String conversion (in-place)
void ebcdic_to_ascii(Byte* data, UInt32 length);
void ascii_to_ebcdic(Byte* data, UInt32 length);

// String conversion (copy)
[[nodiscard]] ByteBuffer ebcdic_to_ascii(const ByteBuffer& ebcdic_data);
[[nodiscard]] ByteBuffer ascii_to_ebcdic(const ByteBuffer& ascii_data);

// String conversion
[[nodiscard]] String ebcdic_to_string(const Byte* ebcdic_data, UInt32 length);
[[nodiscard]] String ebcdic_to_string(const ByteBuffer& ebcdic_data);
[[nodiscard]] ByteBuffer string_to_ebcdic(StringView ascii_string);

// =============================================================================
// Packed Decimal (COMP-3) Operations
// =============================================================================

// Packed decimal structure
struct PackedDecimal {
    ByteBuffer data;
    UInt8 precision = 0;  // Total digits
    UInt8 scale = 0;      // Decimal places
    bool is_valid = false;
    
    [[nodiscard]] Int64 to_int64() const;
    [[nodiscard]] double to_double() const;
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_display() const;  // With formatting
    
    static PackedDecimal from_int64(Int64 value, UInt8 precision = 15, UInt8 scale = 0);
    static PackedDecimal from_double(double value, UInt8 precision = 15, UInt8 scale = 2);
    static PackedDecimal from_string(StringView str, UInt8 precision = 15, UInt8 scale = 0);
};

// Packed decimal validation
[[nodiscard]] bool is_valid_packed(const Byte* data, UInt32 length);
[[nodiscard]] bool is_positive_packed(const Byte* data, UInt32 length);
[[nodiscard]] bool is_negative_packed(const Byte* data, UInt32 length);

// Packed decimal conversion
[[nodiscard]] Int64 packed_to_int64(const Byte* data, UInt32 length);
[[nodiscard]] double packed_to_double(const Byte* data, UInt32 length, UInt8 scale);
[[nodiscard]] String packed_to_string(const Byte* data, UInt32 length, UInt8 scale = 0);

// Integer to packed decimal
void int64_to_packed(Int64 value, Byte* data, UInt32 length);
[[nodiscard]] ByteBuffer int64_to_packed(Int64 value, UInt8 digits);

// String to packed decimal
Result<ByteBuffer> string_to_packed(StringView str, UInt8 digits);

// =============================================================================
// Zoned Decimal Operations
// =============================================================================

// Zoned decimal (DISPLAY NUMERIC) conversion
[[nodiscard]] Int64 zoned_to_int64(const Byte* data, UInt32 length);
[[nodiscard]] String zoned_to_string(const Byte* data, UInt32 length, UInt8 scale = 0);

void int64_to_zoned(Int64 value, Byte* data, UInt32 length);
[[nodiscard]] ByteBuffer int64_to_zoned(Int64 value, UInt8 digits);

// =============================================================================
// Binary (COMP) Operations
// =============================================================================

// Big-endian binary conversion (mainframe format)
[[nodiscard]] Int16 binary_to_int16(const Byte* data);
[[nodiscard]] Int32 binary_to_int32(const Byte* data);
[[nodiscard]] Int64 binary_to_int64(const Byte* data);
[[nodiscard]] UInt16 binary_to_uint16(const Byte* data);
[[nodiscard]] UInt32 binary_to_uint32(const Byte* data);
[[nodiscard]] UInt64 binary_to_uint64(const Byte* data);

void int16_to_binary(Int16 value, Byte* data);
void int32_to_binary(Int32 value, Byte* data);
void int64_to_binary(Int64 value, Byte* data);
void uint16_to_binary(UInt16 value, Byte* data);
void uint32_to_binary(UInt32 value, Byte* data);
void uint64_to_binary(UInt64 value, Byte* data);

// =============================================================================
// Field Manipulation
// =============================================================================

// Move with padding
void move_with_padding(Byte* dest, UInt32 dest_len, const Byte* src, UInt32 src_len,
                       Byte pad_char = 0x40);  // EBCDIC space

// Numeric editing
[[nodiscard]] String edit_numeric(Int64 value, StringView picture);

// =============================================================================
// Utility Functions
// =============================================================================

// Character classification (EBCDIC)
[[nodiscard]] bool is_ebcdic_alpha(Byte c);
[[nodiscard]] bool is_ebcdic_digit(Byte c);
[[nodiscard]] bool is_ebcdic_alnum(Byte c);
[[nodiscard]] bool is_ebcdic_space(Byte c);
[[nodiscard]] bool is_ebcdic_printable(Byte c);

// EBCDIC character constants
constexpr Byte EBCDIC_SPACE = 0x40;
constexpr Byte EBCDIC_ZERO = 0xF0;
constexpr Byte EBCDIC_NINE = 0xF9;
constexpr Byte EBCDIC_A = 0xC1;
constexpr Byte EBCDIC_Z = 0xE9;
constexpr Byte EBCDIC_PLUS = 0x4E;
constexpr Byte EBCDIC_MINUS = 0x60;
constexpr Byte EBCDIC_PERIOD = 0x4B;
constexpr Byte EBCDIC_COMMA = 0x6B;

// Packed decimal sign nibbles
constexpr Byte PACK_POSITIVE_C = 0x0C;  // Preferred positive
constexpr Byte PACK_POSITIVE_A = 0x0A;  // Alternate positive
constexpr Byte PACK_POSITIVE_E = 0x0E;  // Alternate positive
constexpr Byte PACK_POSITIVE_F = 0x0F;  // Unsigned positive
constexpr Byte PACK_NEGATIVE_D = 0x0D;  // Preferred negative
constexpr Byte PACK_NEGATIVE_B = 0x0B;  // Alternate negative

} // namespace ebcdic
} // namespace cics

#endif // CICS_EBCDIC_HPP
