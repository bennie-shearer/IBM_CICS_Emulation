// =============================================================================
// CICS Emulation - EBCDIC Conversion Module Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/ebcdic/ebcdic.hpp>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <cctype>

namespace cics {
namespace ebcdic {

// =============================================================================
// EBCDIC to ASCII Translation Table (IBM-037)
// =============================================================================

const std::array<Byte, 256> EBCDIC_TO_ASCII = {{
    0x00, 0x01, 0x02, 0x03, 0x9C, 0x09, 0x86, 0x7F,  // 00-07
    0x97, 0x8D, 0x8E, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,  // 08-0F
    0x10, 0x11, 0x12, 0x13, 0x9D, 0x85, 0x08, 0x87,  // 10-17
    0x18, 0x19, 0x92, 0x8F, 0x1C, 0x1D, 0x1E, 0x1F,  // 18-1F
    0x80, 0x81, 0x82, 0x83, 0x84, 0x0A, 0x17, 0x1B,  // 20-27
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x05, 0x06, 0x07,  // 28-2F
    0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04,  // 30-37
    0x98, 0x99, 0x9A, 0x9B, 0x14, 0x15, 0x9E, 0x1A,  // 38-3F
    0x20, 0xA0, 0xE2, 0xE4, 0xE0, 0xE1, 0xE3, 0xE5,  // 40-47 (space and special)
    0xE7, 0xF1, 0xA2, 0x2E, 0x3C, 0x28, 0x2B, 0x7C,  // 48-4F
    0x26, 0xE9, 0xEA, 0xEB, 0xE8, 0xED, 0xEE, 0xEF,  // 50-57
    0xEC, 0xDF, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0x5E,  // 58-5F
    0x2D, 0x2F, 0xC2, 0xC4, 0xC0, 0xC1, 0xC3, 0xC5,  // 60-67 (minus, slash)
    0xC7, 0xD1, 0xA6, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,  // 68-6F
    0xF8, 0xC9, 0xCA, 0xCB, 0xC8, 0xCD, 0xCE, 0xCF,  // 70-77
    0xCC, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,  // 78-7F
    0xD8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,  // 80-87 (lowercase a-g)
    0x68, 0x69, 0xAB, 0xBB, 0xF0, 0xFD, 0xFE, 0xB1,  // 88-8F
    0xB0, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70,  // 90-97 (lowercase j-p)
    0x71, 0x72, 0xAA, 0xBA, 0xE6, 0xB8, 0xC6, 0xA4,  // 98-9F
    0xB5, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,  // A0-A7 (lowercase s-x)
    0x79, 0x7A, 0xA1, 0xBF, 0xD0, 0x5B, 0xDE, 0xAE,  // A8-AF
    0xAC, 0xA3, 0xA5, 0xB7, 0xA9, 0xA7, 0xB6, 0xBC,  // B0-B7
    0xBD, 0xBE, 0xDD, 0xA8, 0xAF, 0x5D, 0xB4, 0xD7,  // B8-BF
    0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,  // C0-C7 (uppercase A-G)
    0x48, 0x49, 0xAD, 0xF4, 0xF6, 0xF2, 0xF3, 0xF5,  // C8-CF
    0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,  // D0-D7 (uppercase J-P)
    0x51, 0x52, 0xB9, 0xFB, 0xFC, 0xF9, 0xFA, 0xFF,  // D8-DF
    0x5C, 0xF7, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,  // E0-E7 (uppercase S-X)
    0x59, 0x5A, 0xB2, 0xD4, 0xD6, 0xD2, 0xD3, 0xD5,  // E8-EF
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // F0-F7 (digits 0-7)
    0x38, 0x39, 0xB3, 0xDB, 0xDC, 0xD9, 0xDA, 0x9F   // F8-FF (digits 8-9)
}};

// =============================================================================
// ASCII to EBCDIC Translation Table (IBM-037)
// =============================================================================

const std::array<Byte, 256> ASCII_TO_EBCDIC = {{
    0x00, 0x01, 0x02, 0x03, 0x37, 0x2D, 0x2E, 0x2F,  // 00-07
    0x16, 0x05, 0x25, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,  // 08-0F
    0x10, 0x11, 0x12, 0x13, 0x3C, 0x3D, 0x32, 0x26,  // 10-17
    0x18, 0x19, 0x3F, 0x27, 0x1C, 0x1D, 0x1E, 0x1F,  // 18-1F
    0x40, 0x5A, 0x7F, 0x7B, 0x5B, 0x6C, 0x50, 0x7D,  // 20-27 (space ! " # $ % & ')
    0x4D, 0x5D, 0x5C, 0x4E, 0x6B, 0x60, 0x4B, 0x61,  // 28-2F ( ) * + , - . /
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,  // 30-37 (0-7)
    0xF8, 0xF9, 0x7A, 0x5E, 0x4C, 0x7E, 0x6E, 0x6F,  // 38-3F (8-9 : ; < = > ?)
    0x7C, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,  // 40-47 (@ A-G)
    0xC8, 0xC9, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,  // 48-4F (H-O)
    0xD7, 0xD8, 0xD9, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,  // 50-57 (P-W)
    0xE7, 0xE8, 0xE9, 0xAD, 0xE0, 0xBD, 0x5F, 0x6D,  // 58-5F (X-Z [ \ ] ^ _)
    0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,  // 60-67 (` a-g)
    0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,  // 68-6F (h-o)
    0x97, 0x98, 0x99, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,  // 70-77 (p-w)
    0xA7, 0xA8, 0xA9, 0xC0, 0x4F, 0xD0, 0xA1, 0x07,  // 78-7F (x-z { | } ~)
    0x20, 0x21, 0x22, 0x23, 0x24, 0x15, 0x06, 0x17,  // 80-87
    0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x09, 0x0A, 0x1B,  // 88-8F
    0x30, 0x31, 0x1A, 0x33, 0x34, 0x35, 0x36, 0x08,  // 90-97
    0x38, 0x39, 0x3A, 0x3B, 0x04, 0x14, 0x3E, 0xFF,  // 98-9F
    0x41, 0xAA, 0x4A, 0xB1, 0x9F, 0xB2, 0x6A, 0xB5,  // A0-A7
    0xBB, 0xB4, 0x9A, 0x8A, 0xB0, 0xCA, 0xAF, 0xBC,  // A8-AF
    0x90, 0x8F, 0xEA, 0xFA, 0xBE, 0xA0, 0xB6, 0xB3,  // B0-B7
    0x9D, 0xDA, 0x9B, 0x8B, 0xB7, 0xB8, 0xB9, 0xAB,  // B8-BF
    0x64, 0x65, 0x62, 0x66, 0x63, 0x67, 0x9E, 0x68,  // C0-C7
    0x74, 0x71, 0x72, 0x73, 0x78, 0x75, 0x76, 0x77,  // C8-CF
    0xAC, 0x69, 0xED, 0xEE, 0xEB, 0xEF, 0xEC, 0xBF,  // D0-D7
    0x80, 0xFD, 0xFE, 0xFB, 0xFC, 0xBA, 0xAE, 0x59,  // D8-DF
    0x44, 0x45, 0x42, 0x46, 0x43, 0x47, 0x9C, 0x48,  // E0-E7
    0x54, 0x51, 0x52, 0x53, 0x58, 0x55, 0x56, 0x57,  // E8-EF
    0x8C, 0x49, 0xCD, 0xCE, 0xCB, 0xCF, 0xCC, 0xE1,  // F0-F7
    0x70, 0xDD, 0xDE, 0xDB, 0xDC, 0x8D, 0x8E, 0xDF   // F8-FF
}};

// =============================================================================
// Character Conversion Functions
// =============================================================================

Byte ebcdic_to_ascii(Byte ebcdic_char) {
    return EBCDIC_TO_ASCII[ebcdic_char];
}

Byte ascii_to_ebcdic(Byte ascii_char) {
    return ASCII_TO_EBCDIC[ascii_char];
}

void ebcdic_to_ascii(Byte* data, UInt32 length) {
    for (UInt32 i = 0; i < length; ++i) {
        data[i] = EBCDIC_TO_ASCII[data[i]];
    }
}

void ascii_to_ebcdic(Byte* data, UInt32 length) {
    for (UInt32 i = 0; i < length; ++i) {
        data[i] = ASCII_TO_EBCDIC[data[i]];
    }
}

ByteBuffer ebcdic_to_ascii(const ByteBuffer& ebcdic_data) {
    ByteBuffer result(ebcdic_data.size());
    for (size_t i = 0; i < ebcdic_data.size(); ++i) {
        result[i] = EBCDIC_TO_ASCII[ebcdic_data[i]];
    }
    return result;
}

ByteBuffer ascii_to_ebcdic(const ByteBuffer& ascii_data) {
    ByteBuffer result(ascii_data.size());
    for (size_t i = 0; i < ascii_data.size(); ++i) {
        result[i] = ASCII_TO_EBCDIC[ascii_data[i]];
    }
    return result;
}

String ebcdic_to_string(const Byte* ebcdic_data, UInt32 length) {
    String result(length, '\0');
    for (UInt32 i = 0; i < length; ++i) {
        result[i] = static_cast<char>(EBCDIC_TO_ASCII[ebcdic_data[i]]);
    }
    return result;
}

String ebcdic_to_string(const ByteBuffer& ebcdic_data) {
    return ebcdic_to_string(ebcdic_data.data(), static_cast<UInt32>(ebcdic_data.size()));
}

ByteBuffer string_to_ebcdic(StringView ascii_string) {
    ByteBuffer result(ascii_string.size());
    for (size_t i = 0; i < ascii_string.size(); ++i) {
        result[i] = ASCII_TO_EBCDIC[static_cast<Byte>(ascii_string[i])];
    }
    return result;
}

// =============================================================================
// Packed Decimal Implementation
// =============================================================================

Int64 PackedDecimal::to_int64() const {
    if (!is_valid || data.empty()) return 0;
    return packed_to_int64(data.data(), static_cast<UInt32>(data.size()));
}

double PackedDecimal::to_double() const {
    if (!is_valid || data.empty()) return 0.0;
    return packed_to_double(data.data(), static_cast<UInt32>(data.size()), scale);
}

String PackedDecimal::to_string() const {
    if (!is_valid || data.empty()) return "0";
    return packed_to_string(data.data(), static_cast<UInt32>(data.size()), scale);
}

String PackedDecimal::to_display() const {
    String str = to_string();
    if (scale > 0 && str.length() > scale) {
        str.insert(str.length() - scale, ".");
    }
    return str;
}

PackedDecimal PackedDecimal::from_int64(Int64 value, UInt8 prec, UInt8 scl) {
    PackedDecimal result;
    result.precision = prec;
    result.scale = scl;
    result.data = int64_to_packed(value, static_cast<UInt8>((prec + 2) / 2));
    result.is_valid = true;
    return result;
}

PackedDecimal PackedDecimal::from_double(double value, UInt8 prec, UInt8 scl) {
    Int64 int_value = static_cast<Int64>(value * std::pow(10, scl));
    return from_int64(int_value, prec, scl);
}

PackedDecimal PackedDecimal::from_string(StringView str, UInt8 prec, UInt8 scl) {
    PackedDecimal result;
    result.precision = prec;
    result.scale = scl;
    
    auto packed = string_to_packed(str, static_cast<UInt8>((prec + 2) / 2));
    if (packed.is_success()) {
        result.data = packed.value();
        result.is_valid = true;
    }
    return result;
}

bool is_valid_packed(const Byte* data, UInt32 length) {
    if (!data || length == 0) return false;
    
    // Check all digit nibbles (except last low nibble which is sign)
    for (UInt32 i = 0; i < length - 1; ++i) {
        Byte high = (data[i] >> 4) & 0x0F;
        Byte low = data[i] & 0x0F;
        if (high > 9 || low > 9) return false;
    }
    
    // Check last byte
    Byte high = (data[length - 1] >> 4) & 0x0F;
    Byte sign = data[length - 1] & 0x0F;
    
    if (high > 9) return false;
    if (sign != 0x0C && sign != 0x0D && sign != 0x0F &&
        sign != 0x0A && sign != 0x0B && sign != 0x0E) return false;
    
    return true;
}

bool is_positive_packed(const Byte* data, UInt32 length) {
    if (!data || length == 0) return false;
    Byte sign = data[length - 1] & 0x0F;
    return sign == 0x0C || sign == 0x0A || sign == 0x0E || sign == 0x0F;
}

bool is_negative_packed(const Byte* data, UInt32 length) {
    if (!data || length == 0) return false;
    Byte sign = data[length - 1] & 0x0F;
    return sign == 0x0D || sign == 0x0B;
}

Int64 packed_to_int64(const Byte* data, UInt32 length) {
    if (!data || length == 0) return 0;
    
    Int64 result = 0;
    
    // Process all bytes
    for (UInt32 i = 0; i < length; ++i) {
        Byte high = (data[i] >> 4) & 0x0F;
        Byte low = data[i] & 0x0F;
        
        if (i < length - 1) {
            result = result * 10 + high;
            result = result * 10 + low;
        } else {
            result = result * 10 + high;
            // Low nibble is sign
            if (low == 0x0D || low == 0x0B) {
                result = -result;
            }
        }
    }
    
    return result;
}

double packed_to_double(const Byte* data, UInt32 length, UInt8 scale) {
    Int64 int_val = packed_to_int64(data, length);
    return static_cast<double>(int_val) / std::pow(10, scale);
}

String packed_to_string(const Byte* data, UInt32 length, UInt8 scale) {
    Int64 value = packed_to_int64(data, length);
    bool negative = value < 0;
    if (negative) value = -value;
    
    String result = std::to_string(value);
    
    // Insert decimal point if needed
    if (scale > 0) {
        while (result.length() <= scale) {
            result = "0" + result;
        }
        result.insert(result.length() - scale, ".");
    }
    
    if (negative) {
        result = "-" + result;
    }
    
    return result;
}

void int64_to_packed(Int64 value, Byte* data, UInt32 length) {
    if (!data || length == 0) return;
    
    bool negative = value < 0;
    if (negative) value = -value;
    
    // Clear the output
    std::memset(data, 0, length);
    
    // Set sign nibble
    data[length - 1] = negative ? 0x0D : 0x0C;
    
    // Fill digits from right to left
    UInt32 pos = length - 1;
    bool high_nibble = true;  // Start with high nibble of last byte (before sign)
    
    while (value > 0 && pos < length) {
        Byte digit = static_cast<Byte>(value % 10);
        value /= 10;
        
        if (high_nibble) {
            data[pos] = (data[pos] & 0x0F) | (digit << 4);
            high_nibble = false;
            if (pos > 0) {
                --pos;
                high_nibble = false;  // Next is low nibble of previous byte
            }
        } else {
            if (pos < length - 1) {
                data[pos] = (data[pos] & 0xF0) | digit;
                high_nibble = true;
            }
        }
    }
}

ByteBuffer int64_to_packed(Int64 value, UInt8 digits) {
    UInt32 length = (digits + 2) / 2;  // Each byte holds 2 digits, plus sign
    ByteBuffer result(length, 0);
    int64_to_packed(value, result.data(), length);
    return result;
}

Result<ByteBuffer> string_to_packed(StringView str, UInt8 digits) {
    // Parse the string
    bool negative = false;
    String clean;
    
    for (char c : str) {
        if (c == '-') {
            negative = true;
        } else if (c == '+') {
            negative = false;
        } else if (c >= '0' && c <= '9') {
            clean += c;
        } else if (c != '.' && c != ',') {
            return make_error<ByteBuffer>(ErrorCode::INVALID_ARGUMENT,
                "Invalid character in numeric string");
        }
    }
    
    // Convert to integer
    Int64 value = 0;
    for (char c : clean) {
        value = value * 10 + (c - '0');
    }
    if (negative) value = -value;
    
    return make_success(int64_to_packed(value, digits));
}

// =============================================================================
// Zoned Decimal Implementation
// =============================================================================

Int64 zoned_to_int64(const Byte* data, UInt32 length) {
    if (!data || length == 0) return 0;
    
    Int64 result = 0;
    bool negative = false;
    
    for (UInt32 i = 0; i < length; ++i) {
        Byte zone = (data[i] >> 4) & 0x0F;
        Byte digit = data[i] & 0x0F;
        
        if (digit > 9) digit = 0;
        result = result * 10 + digit;
        
        // Check sign in last byte's zone
        if (i == length - 1) {
            if (zone == 0x0D || zone == 0x0B) {
                negative = true;
            }
        }
    }
    
    return negative ? -result : result;
}

String zoned_to_string(const Byte* data, UInt32 length, UInt8 scale) {
    Int64 value = zoned_to_int64(data, length);
    bool negative = value < 0;
    if (negative) value = -value;
    
    String result = std::to_string(value);
    
    if (scale > 0) {
        while (result.length() <= scale) {
            result = "0" + result;
        }
        result.insert(result.length() - scale, ".");
    }
    
    if (negative) {
        result = "-" + result;
    }
    
    return result;
}

void int64_to_zoned(Int64 value, Byte* data, UInt32 length) {
    if (!data || length == 0) return;
    
    bool negative = value < 0;
    if (negative) value = -value;
    
    // Fill with EBCDIC zeros from right to left
    for (UInt32 i = 0; i < length; ++i) {
        data[i] = 0xF0;  // EBCDIC zero
    }
    
    // Fill digits
    for (UInt32 i = length; i > 0 && value > 0; --i) {
        Byte digit = static_cast<Byte>(value % 10);
        value /= 10;
        data[i - 1] = 0xF0 | digit;
    }
    
    // Set sign in last byte
    if (negative) {
        data[length - 1] = (data[length - 1] & 0x0F) | 0xD0;
    } else {
        data[length - 1] = (data[length - 1] & 0x0F) | 0xC0;
    }
}

ByteBuffer int64_to_zoned(Int64 value, UInt8 digits) {
    ByteBuffer result(digits);
    int64_to_zoned(value, result.data(), digits);
    return result;
}

// =============================================================================
// Binary Conversion Implementation
// =============================================================================

Int16 binary_to_int16(const Byte* data) {
    return static_cast<Int16>((data[0] << 8) | data[1]);
}

Int32 binary_to_int32(const Byte* data) {
    return static_cast<Int32>((data[0] << 24) | (data[1] << 16) | 
                              (data[2] << 8) | data[3]);
}

Int64 binary_to_int64(const Byte* data) {
    return (static_cast<Int64>(data[0]) << 56) | 
           (static_cast<Int64>(data[1]) << 48) |
           (static_cast<Int64>(data[2]) << 40) | 
           (static_cast<Int64>(data[3]) << 32) |
           (static_cast<Int64>(data[4]) << 24) | 
           (static_cast<Int64>(data[5]) << 16) |
           (static_cast<Int64>(data[6]) << 8) | 
           static_cast<Int64>(data[7]);
}

UInt16 binary_to_uint16(const Byte* data) {
    return static_cast<UInt16>((data[0] << 8) | data[1]);
}

UInt32 binary_to_uint32(const Byte* data) {
    return static_cast<UInt32>((data[0] << 24) | (data[1] << 16) | 
                               (data[2] << 8) | data[3]);
}

UInt64 binary_to_uint64(const Byte* data) {
    return (static_cast<UInt64>(data[0]) << 56) | 
           (static_cast<UInt64>(data[1]) << 48) |
           (static_cast<UInt64>(data[2]) << 40) | 
           (static_cast<UInt64>(data[3]) << 32) |
           (static_cast<UInt64>(data[4]) << 24) | 
           (static_cast<UInt64>(data[5]) << 16) |
           (static_cast<UInt64>(data[6]) << 8) | 
           static_cast<UInt64>(data[7]);
}

void int16_to_binary(Int16 value, Byte* data) {
    data[0] = static_cast<Byte>((value >> 8) & 0xFF);
    data[1] = static_cast<Byte>(value & 0xFF);
}

void int32_to_binary(Int32 value, Byte* data) {
    data[0] = static_cast<Byte>((value >> 24) & 0xFF);
    data[1] = static_cast<Byte>((value >> 16) & 0xFF);
    data[2] = static_cast<Byte>((value >> 8) & 0xFF);
    data[3] = static_cast<Byte>(value & 0xFF);
}

void int64_to_binary(Int64 value, Byte* data) {
    data[0] = static_cast<Byte>((value >> 56) & 0xFF);
    data[1] = static_cast<Byte>((value >> 48) & 0xFF);
    data[2] = static_cast<Byte>((value >> 40) & 0xFF);
    data[3] = static_cast<Byte>((value >> 32) & 0xFF);
    data[4] = static_cast<Byte>((value >> 24) & 0xFF);
    data[5] = static_cast<Byte>((value >> 16) & 0xFF);
    data[6] = static_cast<Byte>((value >> 8) & 0xFF);
    data[7] = static_cast<Byte>(value & 0xFF);
}

void uint16_to_binary(UInt16 value, Byte* data) {
    int16_to_binary(static_cast<Int16>(value), data);
}

void uint32_to_binary(UInt32 value, Byte* data) {
    int32_to_binary(static_cast<Int32>(value), data);
}

void uint64_to_binary(UInt64 value, Byte* data) {
    int64_to_binary(static_cast<Int64>(value), data);
}

// =============================================================================
// Field Manipulation
// =============================================================================

void move_with_padding(Byte* dest, UInt32 dest_len, const Byte* src, UInt32 src_len,
                       Byte pad_char) {
    if (!dest || dest_len == 0) return;
    
    UInt32 copy_len = std::min(dest_len, src_len);
    if (src && copy_len > 0) {
        std::memcpy(dest, src, copy_len);
    }
    
    // Pad remainder
    if (copy_len < dest_len) {
        std::memset(dest + copy_len, pad_char, dest_len - copy_len);
    }
}

String edit_numeric(Int64 value, StringView picture) {
    // Simple numeric editing based on picture string
    bool negative = value < 0;
    if (negative) value = -value;
    
    String digits = std::to_string(value);
    String result;
    
    size_t digit_pos = 0;
    for (char c : picture) {
        switch (c) {
            case '9':
            case 'Z':
                if (digit_pos < digits.length()) {
                    result += digits[digit_pos++];
                } else {
                    result += (c == 'Z') ? ' ' : '0';
                }
                break;
            case '.':
            case ',':
            case '-':
            case '+':
            case '$':
                result += c;
                break;
            default:
                result += c;
                break;
        }
    }
    
    return result;
}

// =============================================================================
// Character Classification
// =============================================================================

bool is_ebcdic_alpha(Byte c) {
    return (c >= 0xC1 && c <= 0xC9) ||  // A-I
           (c >= 0xD1 && c <= 0xD9) ||  // J-R
           (c >= 0xE2 && c <= 0xE9) ||  // S-Z
           (c >= 0x81 && c <= 0x89) ||  // a-i
           (c >= 0x91 && c <= 0x99) ||  // j-r
           (c >= 0xA2 && c <= 0xA9);    // s-z
}

bool is_ebcdic_digit(Byte c) {
    return c >= 0xF0 && c <= 0xF9;
}

bool is_ebcdic_alnum(Byte c) {
    return is_ebcdic_alpha(c) || is_ebcdic_digit(c);
}

bool is_ebcdic_space(Byte c) {
    return c == 0x40;
}

bool is_ebcdic_printable(Byte c) {
    return c >= 0x40 && c <= 0xFE;
}

} // namespace ebcdic
} // namespace cics
