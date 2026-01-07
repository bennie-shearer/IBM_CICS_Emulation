#include "cics/common/types.hpp"
#include <algorithm>
#include <cctype>
#include <random>
#include <iomanip>
#include <sstream>

namespace cics {

String to_upper(StringView str) {
    String result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

String to_lower(StringView str) {
    String result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

String trim(StringView str) {
    auto start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == StringView::npos) return "";
    auto end = str.find_last_not_of(" \t\n\r\f\v");
    return String(str.substr(start, end - start + 1));
}

String trim_left(StringView str) {
    auto start = str.find_first_not_of(" \t\n\r\f\v");
    return start == StringView::npos ? "" : String(str.substr(start));
}

String trim_right(StringView str) {
    auto end = str.find_last_not_of(" \t\n\r\f\v");
    return end == StringView::npos ? "" : String(str.substr(0, end + 1));
}

std::vector<String> split(StringView str, char delimiter) {
    std::vector<String> result;
    Size start = 0, end = 0;
    while ((end = str.find(delimiter, start)) != StringView::npos) {
        result.emplace_back(str.substr(start, end - start));
        start = end + 1;
    }
    result.emplace_back(str.substr(start));
    return result;
}

std::vector<String> split(StringView str, StringView delimiter) {
    std::vector<String> result;
    Size start = 0, end = 0;
    while ((end = str.find(delimiter, start)) != StringView::npos) {
        result.emplace_back(str.substr(start, end - start));
        start = end + delimiter.length();
    }
    result.emplace_back(str.substr(start));
    return result;
}

String join(const std::vector<String>& strings, StringView delimiter) {
    if (strings.empty()) return "";
    String result = strings[0];
    for (Size i = 1; i < strings.size(); ++i) {
        result += delimiter;
        result += strings[i];
    }
    return result;
}

bool starts_with(StringView str, StringView prefix) {
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

bool ends_with(StringView str, StringView suffix) {
    return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
}

bool contains(StringView str, StringView substr) {
    return str.find(substr) != StringView::npos;
}

String replace_all(StringView str, StringView from, StringView to) {
    String result(str);
    Size pos = 0;
    while ((pos = result.find(from, pos)) != String::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

String pad_left(StringView str, Size width, char pad) {
    if (str.size() >= width) return String(str);
    return String(width - str.size(), pad) + String(str);
}

String pad_right(StringView str, Size width, char pad) {
    if (str.size() >= width) return String(str);
    return String(str) + String(width - str.size(), pad);
}

static const UInt8 ASCII_TO_EBCDIC[128] = {
    0x00,0x01,0x02,0x03,0x37,0x2D,0x2E,0x2F,0x16,0x05,0x25,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x3C,0x3D,0x32,0x26,0x18,0x19,0x3F,0x27,0x1C,0x1D,0x1E,0x1F,
    0x40,0x5A,0x7F,0x7B,0x5B,0x6C,0x50,0x7D,0x4D,0x5D,0x5C,0x4E,0x6B,0x60,0x4B,0x61,
    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0x7A,0x5E,0x4C,0x7E,0x6E,0x6F,
    0x7C,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,
    0xD7,0xD8,0xD9,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xBA,0xE0,0xBB,0xB0,0x6D,
    0x79,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x91,0x92,0x93,0x94,0x95,0x96,
    0x97,0x98,0x99,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xC0,0x4F,0xD0,0xA1,0x07
};

static const UInt8 EBCDIC_TO_ASCII[256] = {
    0x00,0x01,0x02,0x03,0x9C,0x09,0x86,0x7F,0x97,0x8D,0x8E,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x9D,0x0A,0x08,0x87,0x18,0x19,0x92,0x8F,0x1C,0x1D,0x1E,0x1F,
    0x80,0x81,0x82,0x83,0x84,0x85,0x17,0x1B,0x88,0x89,0x8A,0x8B,0x8C,0x05,0x06,0x07,
    0x90,0x91,0x16,0x93,0x94,0x95,0x96,0x04,0x98,0x99,0x9A,0x9B,0x14,0x15,0x9E,0x1A,
    0x20,0xA0,0xE2,0xE4,0xE0,0xE1,0xE3,0xE5,0xE7,0xF1,0xA2,0x2E,0x3C,0x28,0x2B,0x7C,
    0x26,0xE9,0xEA,0xEB,0xE8,0xED,0xEE,0xEF,0xEC,0xDF,0x21,0x24,0x2A,0x29,0x3B,0x5E,
    0x2D,0x2F,0xC2,0xC4,0xC0,0xC1,0xC3,0xC5,0xC7,0xD1,0xA6,0x2C,0x25,0x5F,0x3E,0x3F,
    0xF8,0xC9,0xCA,0xCB,0xC8,0xCD,0xCE,0xCF,0xCC,0x60,0x3A,0x23,0x40,0x27,0x3D,0x22,
    0xD8,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0xAB,0xBB,0xF0,0xFD,0xFE,0xB1,
    0xB0,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,0x70,0x71,0x72,0xAA,0xBA,0xE6,0xB8,0xC6,0xA4,
    0xB5,0x7E,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0xA1,0xBF,0xD0,0x5B,0xDE,0xAE,
    0xAC,0xA3,0xA5,0xB7,0xA9,0xA7,0xB6,0xBC,0xBD,0xBE,0xDD,0xA8,0xAF,0x5D,0xB4,0xD7,
    0x7B,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0xAD,0xF4,0xF6,0xF2,0xF3,0xF5,
    0x7D,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0xB9,0xFB,0xFC,0xF9,0xFA,0xFF,
    0x5C,0xF7,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0xB2,0xD4,0xD6,0xD2,0xD3,0xD5,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0xB3,0xDB,0xDC,0xD9,0xDA,0x9F
};

EbcdicString ascii_to_ebcdic(StringView ascii) {
    EbcdicString result;
    result.reserve(ascii.size());
    for (unsigned char c : ascii) {
        result.push_back(c < 128 ? ASCII_TO_EBCDIC[c] : 0x3F);
    }
    return result;
}

String ebcdic_to_ascii(ConstByteSpan ebcdic) {
    String result;
    result.reserve(ebcdic.size());
    for (Byte b : ebcdic) {
        result.push_back(static_cast<char>(EBCDIC_TO_ASCII[b]));
    }
    return result;
}

bool is_valid_ebcdic(ConstByteSpan data) {
    for (Byte b : data) {
        char c = static_cast<char>(EBCDIC_TO_ASCII[b]);
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') return false;
    }
    return true;
}

UInt32 crc32(ConstByteSpan data) {
    static const UInt32 TABLE[256] = {
        0x00000000,0x77073096,0xee0e612c,0x990951ba,0x076dc419,0x706af48f,
        0xe963a535,0x9e6495a3,0x0edb8832,0x79dcb8a4,0xe0d5e91e,0x97d2d988,
        0x09b64c2b,0x7eb17cbd,0xe7b82d07,0x90bf1d91,0x1db71064,0x6ab020f2,
        0xf3b97148,0x84be41de,0x1adad47d,0x6ddde4eb,0xf4d4b551,0x83d385c7,
        0x136c9856,0x646ba8c0,0xfd62f97a,0x8a65c9ec,0x14015c4f,0x63066cd9,
        0xfa0f3d63,0x8d080df5,0x3b6e20c8,0x4c69105e,0xd56041e4,0xa2677172,
        0x3c03e4d1,0x4b04d447,0xd20d85fd,0xa50ab56b,0x35b5a8fa,0x42b2986c,
        0xdbbbc9d6,0xacbcf940,0x32d86ce3,0x45df5c75,0xdcd60dcf,0xabd13d59,
        0x26d930ac,0x51de003a,0xc8d75180,0xbfd06116,0x21b4f4b5,0x56b3c423,
        0xcfba9599,0xb8bda50f,0x2802b89e,0x5f058808,0xc60cd9b2,0xb10be924,
        0x2f6f7c87,0x58684c11,0xc1611dab,0xb6662d3d,0x76dc4190,0x01db7106,
        0x98d220bc,0xefd5102a,0x71b18589,0x06b6b51f,0x9fbfe4a5,0xe8b8d433,
        0x7807c9a2,0x0f00f934,0x9609a88e,0xe10e9818,0x7f6a0dbb,0x086d3d2d,
        0x91646c97,0xe6635c01,0x6b6b51f4,0x1c6c6162,0x856530d8,0xf262004e,
        0x6c0695ed,0x1b01a57b,0x8208f4c1,0xf50fc457,0x65b0d9c6,0x12b7e950,
        0x8bbeb8ea,0xfcb9887c,0x62dd1ddf,0x15da2d49,0x8cd37cf3,0xfbd44c65,
        0x4db26158,0x3ab551ce,0xa3bc0074,0xd4bb30e2,0x4adfa541,0x3dd895d7,
        0xa4d1c46d,0xd3d6f4fb,0x4369e96a,0x346ed9fc,0xad678846,0xda60b8d0,
        0x44042d73,0x33031de5,0xaa0a4c5f,0xdd0d7cc9,0x5005713c,0x270241aa,
        0xbe0b1010,0xc90c2086,0x5768b525,0x206f85b3,0xb966d409,0xce61e49f,
        0x5edef90e,0x29d9c998,0xb0d09822,0xc7d7a8b4,0x59b33d17,0x2eb40d81,
        0xb7bd5c3b,0xc0ba6cad,0xedb88320,0x9abfb3b6,0x03b6e20c,0x74b1d29a,
        0xead54739,0x9dd277af,0x04db2615,0x73dc1683,0xe3630b12,0x94643b84,
        0x0d6d6a3e,0x7a6a5aa8,0xe40ecf0b,0x9309ff9d,0x0a00ae27,0x7d079eb1,
        0xf00f9344,0x8708a3d2,0x1e01f268,0x6906c2fe,0xf762575d,0x806567cb,
        0x196c3671,0x6e6b06e7,0xfed41b76,0x89d32be0,0x10da7a5a,0x67dd4acc,
        0xf9b9df6f,0x8ebeeff9,0x17b7be43,0x60b08ed5,0xd6d6a3e8,0xa1d1937e,
        0x38d8c2c4,0x4fdff252,0xd1bb67f1,0xa6bc5767,0x3fb506dd,0x48b2364b,
        0xd80d2bda,0xaf0a1b4c,0x36034af6,0x41047a60,0xdf60efc3,0xa867df55,
        0x316e8eef,0x4669be79,0xcb61b38c,0xbc66831a,0x256fd2a0,0x5268e236,
        0xcc0c7795,0xbb0b4703,0x220216b9,0x5505262f,0xc5ba3bbe,0xb2bd0b28,
        0x2bb45a92,0x5cb36a04,0xc2d7ffa7,0xb5d0cf31,0x2cd99e8b,0x5bdeae1d,
        0x9b64c2b0,0xec63f226,0x756aa39c,0x026d930a,0x9c0906a9,0xeb0e363f,
        0x72076785,0x05005713,0x95bf4a82,0xe2b87a14,0x7bb12bae,0x0cb61b38,
        0x92d28e9b,0xe5d5be0d,0x7cdcefb7,0x0bdbdf21,0x86d3d2d4,0xf1d4e242,
        0x68ddb3f8,0x1fda836e,0x81be16cd,0xf6b9265b,0x6fb077e1,0x18b74777,
        0x88085ae6,0xff0f6a70,0x66063bca,0x11010b5c,0x8f659eff,0xf862ae69,
        0x616bffd3,0x166ccf45,0xa00ae278,0xd70dd2ee,0x4e048354,0x3903b3c2,
        0xa7672661,0xd06016f7,0x4969474d,0x3e6e77db,0xaed16a4a,0xd9d65adc,
        0x40df0b66,0x37d83bf0,0xa9bcae53,0xdebb9ec5,0x47b2cf7f,0x30b5ffe9,
        0xbdbdf21c,0xcabac28a,0x53b39330,0x24b4a3a6,0xbad03605,0xcdd706b3,
        0x54de5729,0x23d967bf,0xb3667a2e,0xc4614ab8,0x5d681b02,0x2a6f2b94,
        0xb40bbe37,0xc30c8ea1,0x5a05df1b,0x2d02ef8d
    };
    UInt32 crc = 0xFFFFFFFF;
    for (Byte b : data) { crc = TABLE[(crc ^ b) & 0xFF] ^ (crc >> 8); }
    return ~crc;
}

UInt64 fnv1a_hash(ConstByteSpan data) {
    UInt64 hash = 14695981039346656037ULL;
    for (Byte b : data) { hash ^= b; hash *= 1099511628211ULL; }
    return hash;
}

String to_hex_string(ConstByteSpan data) {
    static const char HEX[] = "0123456789ABCDEF";
    String result;
    result.reserve(data.size() * 2);
    for (Byte b : data) {
        result.push_back(HEX[(b >> 4) & 0xF]);
        result.push_back(HEX[b & 0xF]);
    }
    return result;
}

ByteBuffer from_hex_string(StringView hex) {
    ByteBuffer result;
    result.reserve(hex.size() / 2);
    auto hex_val = [](char c) -> Byte {
        if (c >= '0' && c <= '9') return static_cast<Byte>(c - '0');
        if (c >= 'A' && c <= 'F') return static_cast<Byte>(10 + c - 'A');
        if (c >= 'a' && c <= 'f') return static_cast<Byte>(10 + c - 'a');
        return 0;
    };
    for (Size i = 0; i + 1 < hex.size(); i += 2) {
        result.push_back(static_cast<Byte>((hex_val(hex[i]) << 4) | hex_val(hex[i + 1])));
    }
    return result;
}

UUID UUID::generate() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<UInt64> dist;
    UUID uuid;
    UInt64* p = reinterpret_cast<UInt64*>(uuid.data.data());
    p[0] = dist(gen); p[1] = dist(gen);
    uuid.data[6] = (uuid.data[6] & 0x0F) | 0x40;
    uuid.data[8] = (uuid.data[8] & 0x3F) | 0x80;
    return uuid;
}

String UUID::to_string() const {
    return std::format("{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],
        data[8],data[9],data[10],data[11],data[12],data[13],data[14],data[15]);
}

bool UUID::is_nil() const {
    return std::all_of(data.begin(), data.end(), [](Byte b) { return b == 0; });
}

void PerformanceMetrics::record_operation(Duration response_time, bool success, Size bytes) {
    total_operations++;
    if (success) successful_operations++; else failed_operations++;
    total_bytes_processed += bytes;
    auto ns = std::chrono::duration_cast<Nanoseconds>(response_time).count();
    total_response_time_ns += static_cast<UInt64>(ns);
    std::lock_guard lock(timing_mutex);
    min_response_time = std::min(min_response_time, response_time);
    max_response_time = std::max(max_response_time, response_time);
}

void PerformanceMetrics::reset() {
    total_operations.reset();
    successful_operations.reset();
    failed_operations.reset();
    total_bytes_processed.reset();
    total_response_time_ns.reset();
    min_response_time = Duration::max();
    max_response_time = Duration::zero();
    start_time = Clock::now();
}

Duration PerformanceMetrics::average_response_time() const {
    auto ops = total_operations.get();
    return ops > 0 ? Nanoseconds(static_cast<Int64>(total_response_time_ns.get() / ops)) : Duration::zero();
}

double PerformanceMetrics::operations_per_second() const {
    auto elapsed = std::chrono::duration_cast<Seconds>(Clock::now() - start_time).count();
    return elapsed > 0 ? static_cast<double>(total_operations.get()) / static_cast<double>(elapsed) : 0.0;
}

double PerformanceMetrics::success_rate() const {
    auto total = total_operations.get();
    return total > 0 ? static_cast<double>(successful_operations.get()) * 100.0 / static_cast<double>(total) : 100.0;
}

double PerformanceMetrics::throughput_mbps() const {
    auto elapsed = std::chrono::duration_cast<Seconds>(Clock::now() - start_time).count();
    return elapsed > 0 ? static_cast<double>(total_bytes_processed.get()) / (static_cast<double>(elapsed) * 1024.0 * 1024.0) : 0.0;
}

String PerformanceMetrics::to_string() const {
    return std::format("Ops: {} ({:.1f}% success), Avg: {}us, Throughput: {:.2f} MB/s",
        total_operations.get(), success_rate(),
        std::chrono::duration_cast<Microseconds>(average_response_time()).count(),
        throughput_mbps());
}

String PerformanceMetrics::to_json() const {
    return std::format(R"({{"total_ops":{},"success_rate":{:.2f},"avg_response_us":{}}})",
        total_operations.get(), success_rate(),
        std::chrono::duration_cast<Microseconds>(average_response_time()).count());
}

Version Version::parse(StringView sv) {
    Version v{}; auto parts = split(sv, '.');
    if (parts.size() >= 1) v.major = static_cast<UInt16>(std::stoi(parts[0]));
    if (parts.size() >= 2) v.minor = static_cast<UInt16>(std::stoi(parts[1]));
    if (parts.size() >= 3) v.patch = static_cast<UInt16>(std::stoi(parts[2]));
    return v;
}

String PackedDecimal::to_string() const {
    if (data_.empty()) return "0";
    String result; bool negative = (data_.back() & 0x0F) == 0x0D;
    for (Size i = 0; i < data_.size(); ++i) {
        UInt8 high = (data_[i] >> 4) & 0x0F;
        if (high < 10) result += static_cast<char>('0' + high);
        if (i < data_.size() - 1) {
            UInt8 low = data_[i] & 0x0F;
            if (low < 10) result += static_cast<char>('0' + low);
        }
    }
    auto first_non_zero = result.find_first_not_of('0');
    if (first_non_zero == String::npos) return "0";
    result = result.substr(first_non_zero);
    if (scale_ > 0 && result.size() > scale_) result.insert(result.size() - scale_, ".");
    return negative ? "-" + result : result;
}

bool PackedDecimal::from_string(StringView str) {
    if (str.empty()) return false;
    String digits; bool negative = false;
    for (char c : str) {
        if (c == '-') negative = true;
        else if (std::isdigit(static_cast<unsigned char>(c))) digits += c;
    }
    if (digits.empty()) return false;
    if (digits.size() % 2 == 0) digits = "0" + digits;
    Size bytes = (digits.size() + 1) / 2;
    data_.clear(); data_.resize(bytes, 0);
    for (Size i = 0; i < digits.size(); ++i) {
        auto digit = static_cast<UInt8>(digits[i] - '0');
        if (i % 2 == 0) data_[i/2] |= static_cast<UInt8>(digit << 4);
        else data_[i/2] |= digit;
    }
    data_.back() = static_cast<UInt8>((data_.back() & 0xF0) | (negative ? 0x0D : 0x0C));
    return true;
}

Int64 PackedDecimal::to_int64() const {
    Int64 result = 0, multiplier = 1;
    for (Size i = data_.size(); i > 0; --i) {
        UInt8 byte = data_[i - 1];
        if (i == data_.size()) {
            result += ((byte >> 4) & 0x0F) * multiplier; multiplier *= 10;
        } else {
            result += (byte & 0x0F) * multiplier; multiplier *= 10;
            result += ((byte >> 4) & 0x0F) * multiplier; multiplier *= 10;
        }
    }
    return is_negative() ? -result : result;
}

Float64 PackedDecimal::to_double() const {
    Float64 result = static_cast<Float64>(to_int64());
    for (UInt8 i = 0; i < scale_; ++i) result /= 10.0;
    return result;
}

bool PackedDecimal::is_positive() const {
    if (data_.empty()) return true;
    UInt8 sign = data_.back() & 0x0F;
    return sign == 0x0C || sign == 0x0A || sign == 0x0E || sign == 0x0F;
}

bool PackedDecimal::is_negative() const {
    if (data_.empty()) return false;
    return (data_.back() & 0x0F) == 0x0D || (data_.back() & 0x0F) == 0x0B;
}

bool PackedDecimal::is_zero() const {
    for (Size i = 0; i < data_.size(); ++i) {
        if (i == data_.size() - 1) { if ((data_[i] & 0xF0) != 0) return false; }
        else { if (data_[i] != 0) return false; }
    }
    return true;
}

PackedDecimal PackedDecimal::abs() const {
    PackedDecimal result = *this;
    if (!result.data_.empty()) result.data_.back() = static_cast<UInt8>((result.data_.back() & 0xF0) | 0x0C);
    return result;
}

PackedDecimal PackedDecimal::negate() const {
    PackedDecimal result = *this;
    if (!result.data_.empty()) {
        UInt8 sign = result.data_.back() & 0x0F;
        result.data_.back() = static_cast<UInt8>((result.data_.back() & 0xF0) | (sign == 0x0D ? 0x0C : 0x0D));
    }
    return result;
}

auto PackedDecimal::operator<=>(const PackedDecimal& other) const { return to_int64() <=> other.to_int64(); }
bool PackedDecimal::operator==(const PackedDecimal& other) const { return to_int64() == other.to_int64(); }

} // namespace cics
