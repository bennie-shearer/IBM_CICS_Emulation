#pragma once
// =============================================================================
// IBM CICS Emulation - UUID Generator
// Version: 3.4.6
// Cross-platform UUID generation for Windows, Linux, and macOS
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <array>
#include <random>
#include <chrono>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace cics::uuid {

// =============================================================================
// UUID Class
// =============================================================================

class UUID {
public:
    static constexpr Size SIZE = 16;
    using Data = std::array<Byte, SIZE>;

private:
    Data data_{};

public:
    // Constructors
    UUID() = default;
    explicit UUID(const Data& data) : data_(data) {}
    
    // Generate new UUIDs
    [[nodiscard]] static UUID generate();
    [[nodiscard]] static UUID generate_v4();  // Random UUID
    [[nodiscard]] static UUID nil();          // All zeros
    
    // Parse from string
    [[nodiscard]] static Result<UUID> parse(StringView str);
    
    // Accessors
    [[nodiscard]] const Data& data() const { return data_; }
    [[nodiscard]] Data& data() { return data_; }
    [[nodiscard]] const Byte* bytes() const { return data_.data(); }
    [[nodiscard]] Byte* bytes() { return data_.data(); }
    
    // Properties
    [[nodiscard]] bool is_nil() const;
    [[nodiscard]] int version() const;
    [[nodiscard]] int variant() const;
    
    // String representation
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_string_upper() const;
    [[nodiscard]] String to_string_no_dashes() const;
    
    // Comparison
    [[nodiscard]] bool operator==(const UUID& other) const { return data_ == other.data_; }
    [[nodiscard]] bool operator!=(const UUID& other) const { return data_ != other.data_; }
    [[nodiscard]] bool operator<(const UUID& other) const { return data_ < other.data_; }
    [[nodiscard]] bool operator<=(const UUID& other) const { return data_ <= other.data_; }
    [[nodiscard]] bool operator>(const UUID& other) const { return data_ > other.data_; }
    [[nodiscard]] bool operator>=(const UUID& other) const { return data_ >= other.data_; }
    
    // Hash support
    [[nodiscard]] Size hash() const;
};

// =============================================================================
// UUID Implementation
// =============================================================================

inline UUID UUID::generate() {
    return generate_v4();
}

inline UUID UUID::generate_v4() {
    UUID uuid;
    
    // Use high-quality random number generator
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<UInt64> dist;
    
    // Generate random bytes
    UInt64 r1 = dist(gen);
    UInt64 r2 = dist(gen);
    
    std::memcpy(uuid.data_.data(), &r1, 8);
    std::memcpy(uuid.data_.data() + 8, &r2, 8);
    
    // Set version 4 (random)
    uuid.data_[6] = (uuid.data_[6] & 0x0F) | 0x40;
    
    // Set variant (RFC 4122)
    uuid.data_[8] = (uuid.data_[8] & 0x3F) | 0x80;
    
    return uuid;
}

inline UUID UUID::nil() {
    return UUID();
}

inline Result<UUID> UUID::parse(StringView str) {
    // Remove dashes if present
    String clean;
    clean.reserve(32);
    
    for (char c : str) {
        if (c != '-') {
            clean += c;
        }
    }
    
    if (clean.size() != 32) {
        return make_error<UUID>(ErrorCode::INVREQ, "Invalid UUID length");
    }
    
    UUID uuid;
    for (Size i = 0; i < 16; ++i) {
        unsigned char high = static_cast<unsigned char>(clean[i * 2]);
        unsigned char low = static_cast<unsigned char>(clean[i * 2 + 1]);
        
        auto hex_val = [](unsigned char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        
        int h = hex_val(high);
        int l = hex_val(low);
        if (h < 0 || l < 0) {
            return make_error<UUID>(ErrorCode::INVREQ, "Invalid hex character");
        }
        
        uuid.data_[i] = static_cast<Byte>((h << 4) | l);
    }
    
    return uuid;
}

inline bool UUID::is_nil() const {
    for (Byte b : data_) {
        if (b != 0) return false;
    }
    return true;
}

inline int UUID::version() const {
    return (data_[6] >> 4) & 0x0F;
}

inline int UUID::variant() const {
    Byte b = data_[8];
    if ((b & 0x80) == 0x00) return 0;      // NCS backward compatibility
    if ((b & 0xC0) == 0x80) return 1;      // RFC 4122
    if ((b & 0xE0) == 0xC0) return 2;      // Microsoft backward compatibility
    return 3;                               // Reserved
}

inline String UUID::to_string() const {
    static const char hex[] = "0123456789abcdef";
    String result;
    result.reserve(36);
    
    for (Size i = 0; i < SIZE; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            result += '-';
        }
        result += hex[(data_[i] >> 4) & 0x0F];
        result += hex[data_[i] & 0x0F];
    }
    
    return result;
}

inline String UUID::to_string_upper() const {
    static const char hex[] = "0123456789ABCDEF";
    String result;
    result.reserve(36);
    
    for (Size i = 0; i < SIZE; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            result += '-';
        }
        result += hex[(data_[i] >> 4) & 0x0F];
        result += hex[data_[i] & 0x0F];
    }
    
    return result;
}

inline String UUID::to_string_no_dashes() const {
    static const char hex[] = "0123456789abcdef";
    String result;
    result.reserve(32);
    
    for (Size i = 0; i < SIZE; ++i) {
        result += hex[(data_[i] >> 4) & 0x0F];
        result += hex[data_[i] & 0x0F];
    }
    
    return result;
}

inline Size UUID::hash() const {
    Size h = 0;
    for (Size i = 0; i < SIZE; ++i) {
        h ^= static_cast<Size>(data_[i]) << ((i % sizeof(Size)) * 8);
    }
    return h;
}

// =============================================================================
// UUID Generator Class (for bulk generation)
// =============================================================================

class UUIDGenerator {
private:
    std::mt19937_64 gen_;
    std::uniform_int_distribution<UInt64> dist_;
    
public:
    UUIDGenerator() {
        std::random_device rd;
        gen_.seed(rd());
    }
    
    explicit UUIDGenerator(UInt64 seed) : gen_(seed) {}
    
    [[nodiscard]] UUID generate() {
        UUID uuid;
        
        UInt64 r1 = dist_(gen_);
        UInt64 r2 = dist_(gen_);
        
        std::memcpy(uuid.data().data(), &r1, 8);
        std::memcpy(uuid.data().data() + 8, &r2, 8);
        
        // Set version 4 (random)
        uuid.data()[6] = (uuid.data()[6] & 0x0F) | 0x40;
        
        // Set variant (RFC 4122)
        uuid.data()[8] = (uuid.data()[8] & 0x3F) | 0x80;
        
        return uuid;
    }
    
    [[nodiscard]] Vector<UUID> generate_batch(Size count) {
        Vector<UUID> result;
        result.reserve(count);
        for (Size i = 0; i < count; ++i) {
            result.push_back(generate());
        }
        return result;
    }
};

// =============================================================================
// Helper Functions
// =============================================================================

// Generate a new UUID
[[nodiscard]] inline UUID make_uuid() {
    return UUID::generate();
}

// Parse UUID from string
[[nodiscard]] inline Result<UUID> parse_uuid(StringView str) {
    return UUID::parse(str);
}

// Check if string is valid UUID
[[nodiscard]] inline bool is_valid_uuid(StringView str) {
    return UUID::parse(str).is_success();
}

} // namespace cics::uuid

// Hash specialization for std::hash
namespace std {
template<>
struct hash<cics::uuid::UUID> {
    size_t operator()(const cics::uuid::UUID& uuid) const noexcept {
        return uuid.hash();
    }
};
} // namespace std
