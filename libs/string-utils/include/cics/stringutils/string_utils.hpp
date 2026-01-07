#pragma once
// =============================================================================
// IBM CICS Emulation - String Utilities
// Version: 3.4.6
// Cross-platform string manipulation for Windows, Linux, and macOS
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

namespace cics::stringutils {

// =============================================================================
// Trimming Functions
// =============================================================================

// Trim whitespace from left
[[nodiscard]] inline String ltrim(StringView str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    return String(str.substr(start));
}

// Trim whitespace from right
[[nodiscard]] inline String rtrim(StringView str) {
    size_t end = str.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return String(str.substr(0, end));
}

// Trim whitespace from both sides
[[nodiscard]] inline String trim(StringView str) {
    return ltrim(rtrim(str));
}

// Trim specific characters
[[nodiscard]] inline String trim_chars(StringView str, StringView chars) {
    size_t start = str.find_first_not_of(chars);
    if (start == StringView::npos) return "";
    size_t end = str.find_last_not_of(chars);
    return String(str.substr(start, end - start + 1));
}

// =============================================================================
// Case Conversion
// =============================================================================

// Convert to uppercase
[[nodiscard]] inline String to_upper(StringView str) {
    String result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

// Convert to lowercase
[[nodiscard]] inline String to_lower(StringView str) {
    String result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

// Capitalize first letter
[[nodiscard]] inline String capitalize(StringView str) {
    if (str.empty()) return "";
    String result(str);
    result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
    return result;
}

// Title case (capitalize each word)
[[nodiscard]] inline String title_case(StringView str) {
    String result(str);
    bool new_word = true;
    for (char& c : result) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            new_word = true;
        } else if (new_word) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            new_word = false;
        } else {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return result;
}

// =============================================================================
// Splitting and Joining
// =============================================================================

// Split string by delimiter
[[nodiscard]] inline Vector<String> split(StringView str, char delimiter) {
    Vector<String> result;
    String str_copy{str};
    std::istringstream iss{str_copy};
    String token;
    while (std::getline(iss, token, delimiter)) {
        result.push_back(std::move(token));
    }
    return result;
}

// Split string by string delimiter
[[nodiscard]] inline Vector<String> split(StringView str, StringView delimiter) {
    Vector<String> result;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != StringView::npos) {
        result.push_back(String(str.substr(start, end - start)));
        start = end + delimiter.size();
        end = str.find(delimiter, start);
    }
    result.push_back(String(str.substr(start)));
    return result;
}

// Split into lines
[[nodiscard]] inline Vector<String> split_lines(StringView str) {
    Vector<String> result;
    String str_copy{str};
    std::istringstream iss{str_copy};
    String line;
    while (std::getline(iss, line)) {
        // Remove trailing \r if present (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        result.push_back(std::move(line));
    }
    return result;
}

// Join strings with delimiter
[[nodiscard]] inline String join(const Vector<String>& parts, StringView delimiter) {
    if (parts.empty()) return "";
    
    std::ostringstream oss;
    oss << parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        oss << delimiter << parts[i];
    }
    return oss.str();
}

// =============================================================================
// Search and Replace
// =============================================================================

// Check if string contains substring
[[nodiscard]] inline bool contains(StringView str, StringView substr) {
    return str.find(substr) != StringView::npos;
}

// Check if string starts with prefix
[[nodiscard]] inline bool starts_with(StringView str, StringView prefix) {
    if (prefix.size() > str.size()) return false;
    return str.substr(0, prefix.size()) == prefix;
}

// Check if string ends with suffix
[[nodiscard]] inline bool ends_with(StringView str, StringView suffix) {
    if (suffix.size() > str.size()) return false;
    return str.substr(str.size() - suffix.size()) == suffix;
}

// Replace all occurrences
[[nodiscard]] inline String replace_all(StringView str, StringView from, StringView to) {
    if (from.empty()) return String(str);
    
    String result;
    result.reserve(str.size());
    
    size_t start = 0;
    size_t pos;
    while ((pos = str.find(from, start)) != StringView::npos) {
        result.append(str.data() + start, pos - start);
        result.append(to);
        start = pos + from.size();
    }
    result.append(str.data() + start, str.size() - start);
    return result;
}

// Replace first occurrence
[[nodiscard]] inline String replace_first(StringView str, StringView from, StringView to) {
    size_t pos = str.find(from);
    if (pos == StringView::npos) return String(str);
    
    String result(str);
    result.replace(pos, from.size(), to);
    return result;
}

// Count occurrences
[[nodiscard]] inline Size count(StringView str, StringView substr) {
    if (substr.empty()) return 0;
    
    Size count = 0;
    size_t pos = 0;
    while ((pos = str.find(substr, pos)) != StringView::npos) {
        ++count;
        pos += substr.size();
    }
    return count;
}

// =============================================================================
// Padding and Alignment
// =============================================================================

// Pad left to width
[[nodiscard]] inline String pad_left(StringView str, Size width, char pad_char = ' ') {
    if (str.size() >= width) return String(str);
    return String(width - str.size(), pad_char) + String(str);
}

// Pad right to width
[[nodiscard]] inline String pad_right(StringView str, Size width, char pad_char = ' ') {
    if (str.size() >= width) return String(str);
    return String(str) + String(width - str.size(), pad_char);
}

// Center string
[[nodiscard]] inline String center(StringView str, Size width, char pad_char = ' ') {
    if (str.size() >= width) return String(str);
    Size total_pad = width - str.size();
    Size left_pad = total_pad / 2;
    Size right_pad = total_pad - left_pad;
    return String(left_pad, pad_char) + String(str) + String(right_pad, pad_char);
}

// Truncate to max length
[[nodiscard]] inline String truncate(StringView str, Size max_len, StringView suffix = "...") {
    if (str.size() <= max_len) return String(str);
    if (max_len <= suffix.size()) return String(str.substr(0, max_len));
    return String(str.substr(0, max_len - suffix.size())) + String(suffix);
}

// =============================================================================
// Character Operations
// =============================================================================

// Remove all occurrences of character
[[nodiscard]] inline String remove_char(StringView str, char ch) {
    String result;
    result.reserve(str.size());
    for (char c : str) {
        if (c != ch) result += c;
    }
    return result;
}

// Remove all occurrences of characters in set
[[nodiscard]] inline String remove_chars(StringView str, StringView chars) {
    String result;
    result.reserve(str.size());
    for (char c : str) {
        if (chars.find(c) == StringView::npos) {
            result += c;
        }
    }
    return result;
}

// Keep only alphanumeric characters
[[nodiscard]] inline String keep_alnum(StringView str) {
    String result;
    result.reserve(str.size());
    for (char c : str) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            result += c;
        }
    }
    return result;
}

// Keep only printable characters
[[nodiscard]] inline String keep_printable(StringView str) {
    String result;
    result.reserve(str.size());
    for (char c : str) {
        if (std::isprint(static_cast<unsigned char>(c))) {
            result += c;
        }
    }
    return result;
}

// =============================================================================
// Conversion Functions
// =============================================================================

// String to integer with default
[[nodiscard]] inline Int64 to_int(StringView str, Int64 default_val = 0) {
    if (str.empty()) return default_val;
    try {
        return std::stoll(String(str));
    } catch (...) {
        return default_val;
    }
}

// String to unsigned integer with default
[[nodiscard]] inline UInt64 to_uint(StringView str, UInt64 default_val = 0) {
    if (str.empty()) return default_val;
    try {
        return std::stoull(String(str));
    } catch (...) {
        return default_val;
    }
}

// String to double with default
[[nodiscard]] inline Float64 to_double(StringView str, Float64 default_val = 0.0) {
    if (str.empty()) return default_val;
    try {
        return std::stod(String(str));
    } catch (...) {
        return default_val;
    }
}

// String to bool
[[nodiscard]] inline bool to_bool(StringView str, bool default_val = false) {
    String lower = to_lower(trim(str));
    if (lower == "true" || lower == "yes" || lower == "on" || lower == "1") return true;
    if (lower == "false" || lower == "no" || lower == "off" || lower == "0") return false;
    return default_val;
}

// =============================================================================
// Hex Encoding
// =============================================================================

// Convert bytes to hex string
[[nodiscard]] inline String to_hex(ConstByteSpan data) {
    static const char hex_chars[] = "0123456789ABCDEF";
    String result;
    result.reserve(data.size() * 2);
    for (Byte b : data) {
        result += hex_chars[(b >> 4) & 0x0F];
        result += hex_chars[b & 0x0F];
    }
    return result;
}

// Convert hex string to bytes
[[nodiscard]] inline Result<ByteBuffer> from_hex(StringView hex) {
    if (hex.size() % 2 != 0) {
        return make_error<ByteBuffer>(ErrorCode::INVREQ, "Invalid hex string length");
    }
    
    ByteBuffer result;
    result.reserve(hex.size() / 2);
    
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned char high = static_cast<unsigned char>(hex[i]);
        unsigned char low = static_cast<unsigned char>(hex[i + 1]);
        
        auto hex_val = [](unsigned char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        
        int h = hex_val(high);
        int l = hex_val(low);
        if (h < 0 || l < 0) {
            return make_error<ByteBuffer>(ErrorCode::INVREQ, "Invalid hex character");
        }
        
        result.push_back(static_cast<Byte>((h << 4) | l));
    }
    
    return result;
}

// =============================================================================
// Formatting Functions
// =============================================================================

// Format number with thousands separator
[[nodiscard]] inline String format_number(Int64 value, char separator = ',') {
    String str = std::to_string(std::abs(value));
    String result;
    int count = 0;
    
    for (auto it = str.rbegin(); it != str.rend(); ++it) {
        if (count > 0 && count % 3 == 0) {
            result = separator + result;
        }
        result = *it + result;
        ++count;
    }
    
    if (value < 0) result = '-' + result;
    return result;
}

// Format bytes as human-readable size
[[nodiscard]] inline String format_bytes(UInt64 bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unit_idx = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_idx < 5) {
        size /= 1024.0;
        ++unit_idx;
    }
    
    std::ostringstream oss;
    if (unit_idx == 0) {
        oss << bytes << " " << units[unit_idx];
    } else {
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit_idx];
    }
    return oss.str();
}

// Repeat string n times
[[nodiscard]] inline String repeat(StringView str, Size count) {
    String result;
    result.reserve(str.size() * count);
    for (Size i = 0; i < count; ++i) {
        result.append(str);
    }
    return result;
}

// Reverse string
[[nodiscard]] inline String reverse(StringView str) {
    return String(str.rbegin(), str.rend());
}

// =============================================================================
// Comparison Functions
// =============================================================================

// Case-insensitive comparison
[[nodiscard]] inline bool equals_ignore_case(StringView a, StringView b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != 
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// Natural sort comparison (handles numbers in strings)
[[nodiscard]] inline int compare_natural(StringView a, StringView b);

// =============================================================================
// CICS-Specific String Functions
// =============================================================================

// Convert to valid CICS name (uppercase, 1-8 chars, alphanumeric)
[[nodiscard]] inline String to_cics_name(StringView str, Size max_len = 8) {
    String result;
    result.reserve(max_len);
    
    for (char c : str) {
        if (result.size() >= max_len) break;
        if (std::isalnum(static_cast<unsigned char>(c)) || 
            c == '@' || c == '#' || c == '$') {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    return result;
}

// Pad string to fixed length (mainframe style)
[[nodiscard]] inline String fixed_length(StringView str, Size len, char pad = ' ') {
    if (str.size() >= len) {
        return String(str.substr(0, len));
    }
    return String(str) + String(len - str.size(), pad);
}

} // namespace cics::stringutils
