#pragma once
// =============================================================================
// IBM CICS Emulation - Serialization Utilities
// Version: 3.4.6
// Binary serialization for Windows, Linux, and macOS
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <cstring>

namespace cics::serialization {

// =============================================================================
// Byte Order
// =============================================================================

enum class ByteOrder {
    LITTLE_ENDIAN_ORDER,  // x86, x64
    BIG_ENDIAN_ORDER      // Mainframe, network
};

// Detect native byte order
[[nodiscard]] inline ByteOrder native_byte_order() {
    union { UInt32 i; Byte c[4]; } test = {0x01020304};
    return (test.c[0] == 0x01) ? ByteOrder::BIG_ENDIAN_ORDER : ByteOrder::LITTLE_ENDIAN_ORDER;
}

// Byte swap functions
[[nodiscard]] inline UInt16 swap_bytes(UInt16 value) {
    return (value >> 8) | (value << 8);
}

[[nodiscard]] inline UInt32 swap_bytes(UInt32 value) {
    return ((value >> 24) & 0x000000FF) |
           ((value >> 8)  & 0x0000FF00) |
           ((value << 8)  & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
}

[[nodiscard]] inline UInt64 swap_bytes(UInt64 value) {
    return ((value >> 56) & 0x00000000000000FFULL) |
           ((value >> 40) & 0x000000000000FF00ULL) |
           ((value >> 24) & 0x0000000000FF0000ULL) |
           ((value >> 8)  & 0x00000000FF000000ULL) |
           ((value << 8)  & 0x000000FF00000000ULL) |
           ((value << 24) & 0x0000FF0000000000ULL) |
           ((value << 40) & 0x00FF000000000000ULL) |
           ((value << 56) & 0xFF00000000000000ULL);
}

// Convert to/from network byte order (big-endian)
template<typename T>
[[nodiscard]] inline T to_network_order(T value) {
    if (native_byte_order() == ByteOrder::LITTLE_ENDIAN_ORDER) {
        if constexpr (sizeof(T) == 2) return static_cast<T>(swap_bytes(static_cast<UInt16>(value)));
        if constexpr (sizeof(T) == 4) return static_cast<T>(swap_bytes(static_cast<UInt32>(value)));
        if constexpr (sizeof(T) == 8) return static_cast<T>(swap_bytes(static_cast<UInt64>(value)));
    }
    return value;
}

template<typename T>
[[nodiscard]] inline T from_network_order(T value) {
    return to_network_order(value);  // Symmetric operation
}

// =============================================================================
// Binary Writer
// =============================================================================

class BinaryWriter {
private:
    ByteBuffer buffer_;
    ByteOrder byte_order_ = ByteOrder::LITTLE_ENDIAN_ORDER;
    
public:
    BinaryWriter() = default;
    explicit BinaryWriter(Size initial_capacity) { buffer_.reserve(initial_capacity); }
    explicit BinaryWriter(ByteOrder order) : byte_order_(order) {}
    
    // Byte order
    void set_byte_order(ByteOrder order) { byte_order_ = order; }
    [[nodiscard]] ByteOrder byte_order() const { return byte_order_; }
    
    // Buffer access
    [[nodiscard]] const ByteBuffer& buffer() const { return buffer_; }
    [[nodiscard]] ByteBuffer&& take_buffer() { return std::move(buffer_); }
    [[nodiscard]] Size size() const { return buffer_.size(); }
    void clear() { buffer_.clear(); }
    void reserve(Size capacity) { buffer_.reserve(capacity); }
    
    // Write raw bytes
    void write_bytes(ConstByteSpan data) {
        buffer_.insert(buffer_.end(), data.begin(), data.end());
    }
    
    void write_bytes(const void* data, Size size) {
        const Byte* ptr = static_cast<const Byte*>(data);
        buffer_.insert(buffer_.end(), ptr, ptr + size);
    }
    
    // Write single byte
    void write_byte(Byte value) {
        buffer_.push_back(value);
    }
    
    // Write integers (respects byte order)
    void write_int8(Int8 value) {
        buffer_.push_back(static_cast<Byte>(value));
    }
    
    void write_uint8(UInt8 value) {
        buffer_.push_back(value);
    }
    
    void write_int16(Int16 value) {
        UInt16 v = static_cast<UInt16>(value);
        if (byte_order_ == ByteOrder::BIG_ENDIAN_ORDER && native_byte_order() == ByteOrder::LITTLE_ENDIAN_ORDER) {
            v = swap_bytes(v);
        } else if (byte_order_ == ByteOrder::LITTLE_ENDIAN_ORDER && native_byte_order() == ByteOrder::BIG_ENDIAN_ORDER) {
            v = swap_bytes(v);
        }
        write_bytes(&v, sizeof(v));
    }
    
    void write_uint16(UInt16 value) {
        write_int16(static_cast<Int16>(value));
    }
    
    void write_int32(Int32 value) {
        UInt32 v = static_cast<UInt32>(value);
        if (byte_order_ == ByteOrder::BIG_ENDIAN_ORDER && native_byte_order() == ByteOrder::LITTLE_ENDIAN_ORDER) {
            v = swap_bytes(v);
        } else if (byte_order_ == ByteOrder::LITTLE_ENDIAN_ORDER && native_byte_order() == ByteOrder::BIG_ENDIAN_ORDER) {
            v = swap_bytes(v);
        }
        write_bytes(&v, sizeof(v));
    }
    
    void write_uint32(UInt32 value) {
        write_int32(static_cast<Int32>(value));
    }
    
    void write_int64(Int64 value) {
        UInt64 v = static_cast<UInt64>(value);
        if (byte_order_ == ByteOrder::BIG_ENDIAN_ORDER && native_byte_order() == ByteOrder::LITTLE_ENDIAN_ORDER) {
            v = swap_bytes(v);
        } else if (byte_order_ == ByteOrder::LITTLE_ENDIAN_ORDER && native_byte_order() == ByteOrder::BIG_ENDIAN_ORDER) {
            v = swap_bytes(v);
        }
        write_bytes(&v, sizeof(v));
    }
    
    void write_uint64(UInt64 value) {
        write_int64(static_cast<Int64>(value));
    }
    
    // Write floating point
    void write_float(Float32 value) {
        UInt32 v;
        std::memcpy(&v, &value, sizeof(v));
        write_uint32(v);
    }
    
    void write_double(Float64 value) {
        UInt64 v;
        std::memcpy(&v, &value, sizeof(v));
        write_uint64(v);
    }
    
    // Write boolean
    void write_bool(bool value) {
        write_byte(value ? 1 : 0);
    }
    
    // Write string (length-prefixed)
    void write_string(StringView str) {
        write_uint32(static_cast<UInt32>(str.size()));
        write_bytes(str.data(), str.size());
    }
    
    // Write fixed-length string (padded or truncated)
    void write_fixed_string(StringView str, Size length, char pad = '\0') {
        Size write_len = std::min(str.size(), length);
        write_bytes(str.data(), write_len);
        for (Size i = write_len; i < length; ++i) {
            write_byte(static_cast<Byte>(pad));
        }
    }
    
    // Write null-terminated string
    void write_cstring(StringView str) {
        write_bytes(str.data(), str.size());
        write_byte(0);
    }
};

// =============================================================================
// Binary Reader
// =============================================================================

class BinaryReader {
private:
    ConstByteSpan data_;
    Size position_ = 0;
    ByteOrder byte_order_ = ByteOrder::LITTLE_ENDIAN_ORDER;
    
public:
    BinaryReader() = default;
    explicit BinaryReader(ConstByteSpan data) : data_(data) {}
    BinaryReader(ConstByteSpan data, ByteOrder order) : data_(data), byte_order_(order) {}
    BinaryReader(const ByteBuffer& buffer) : data_(buffer) {}
    
    // Byte order
    void set_byte_order(ByteOrder order) { byte_order_ = order; }
    [[nodiscard]] ByteOrder byte_order() const { return byte_order_; }
    
    // Position and size
    [[nodiscard]] Size position() const { return position_; }
    [[nodiscard]] Size size() const { return data_.size(); }
    [[nodiscard]] Size remaining() const { return data_.size() - position_; }
    [[nodiscard]] bool eof() const { return position_ >= data_.size(); }
    
    void seek(Size pos) { position_ = std::min(pos, data_.size()); }
    void skip(Size count) { position_ = std::min(position_ + count, data_.size()); }
    void reset() { position_ = 0; }
    
    // Read raw bytes
    [[nodiscard]] Result<void> read_bytes(ByteSpan dest) {
        if (remaining() < dest.size()) {
            return make_error<void>(ErrorCode::ENDFILE, "Not enough data");
        }
        std::memcpy(dest.data(), data_.data() + position_, dest.size());
        position_ += dest.size();
        return {};
    }
    
    [[nodiscard]] Result<ByteBuffer> read_bytes(Size count) {
        if (remaining() < count) {
            return make_error<ByteBuffer>(ErrorCode::ENDFILE, "Not enough data");
        }
        ByteBuffer result(data_.begin() + position_, data_.begin() + position_ + count);
        position_ += count;
        return result;
    }
    
    // Read single byte
    [[nodiscard]] Result<Byte> read_byte() {
        if (eof()) {
            return make_error<Byte>(ErrorCode::ENDFILE, "End of data");
        }
        return data_[position_++];
    }
    
    // Read integers
    [[nodiscard]] Result<Int8> read_int8() {
        auto b = read_byte();
        if (!b.is_success()) return make_error<Int8>(b.error().code, b.error().message);
        return static_cast<Int8>(b.value());
    }
    
    [[nodiscard]] Result<UInt8> read_uint8() {
        return read_byte();
    }
    
    [[nodiscard]] Result<Int16> read_int16() {
        if (remaining() < 2) {
            return make_error<Int16>(ErrorCode::ENDFILE, "Not enough data");
        }
        UInt16 v;
        std::memcpy(&v, data_.data() + position_, sizeof(v));
        position_ += sizeof(v);
        
        if (byte_order_ == ByteOrder::BIG_ENDIAN_ORDER && native_byte_order() == ByteOrder::LITTLE_ENDIAN_ORDER) {
            v = swap_bytes(v);
        } else if (byte_order_ == ByteOrder::LITTLE_ENDIAN_ORDER && native_byte_order() == ByteOrder::BIG_ENDIAN_ORDER) {
            v = swap_bytes(v);
        }
        return static_cast<Int16>(v);
    }
    
    [[nodiscard]] Result<UInt16> read_uint16() {
        auto v = read_int16();
        if (!v.is_success()) return make_error<UInt16>(v.error().code, v.error().message);
        return static_cast<UInt16>(v.value());
    }
    
    [[nodiscard]] Result<Int32> read_int32() {
        if (remaining() < 4) {
            return make_error<Int32>(ErrorCode::ENDFILE, "Not enough data");
        }
        UInt32 v;
        std::memcpy(&v, data_.data() + position_, sizeof(v));
        position_ += sizeof(v);
        
        if (byte_order_ == ByteOrder::BIG_ENDIAN_ORDER && native_byte_order() == ByteOrder::LITTLE_ENDIAN_ORDER) {
            v = swap_bytes(v);
        } else if (byte_order_ == ByteOrder::LITTLE_ENDIAN_ORDER && native_byte_order() == ByteOrder::BIG_ENDIAN_ORDER) {
            v = swap_bytes(v);
        }
        return static_cast<Int32>(v);
    }
    
    [[nodiscard]] Result<UInt32> read_uint32() {
        auto v = read_int32();
        if (!v.is_success()) return make_error<UInt32>(v.error().code, v.error().message);
        return static_cast<UInt32>(v.value());
    }
    
    [[nodiscard]] Result<Int64> read_int64() {
        if (remaining() < 8) {
            return make_error<Int64>(ErrorCode::ENDFILE, "Not enough data");
        }
        UInt64 v;
        std::memcpy(&v, data_.data() + position_, sizeof(v));
        position_ += sizeof(v);
        
        if (byte_order_ == ByteOrder::BIG_ENDIAN_ORDER && native_byte_order() == ByteOrder::LITTLE_ENDIAN_ORDER) {
            v = swap_bytes(v);
        } else if (byte_order_ == ByteOrder::LITTLE_ENDIAN_ORDER && native_byte_order() == ByteOrder::BIG_ENDIAN_ORDER) {
            v = swap_bytes(v);
        }
        return static_cast<Int64>(v);
    }
    
    [[nodiscard]] Result<UInt64> read_uint64() {
        auto v = read_int64();
        if (!v.is_success()) return make_error<UInt64>(v.error().code, v.error().message);
        return static_cast<UInt64>(v.value());
    }
    
    // Read floating point
    [[nodiscard]] Result<Float32> read_float() {
        auto v = read_uint32();
        if (!v.is_success()) return make_error<Float32>(v.error().code, v.error().message);
        Float32 result;
        UInt32 raw = v.value();
        std::memcpy(&result, &raw, sizeof(result));
        return result;
    }
    
    [[nodiscard]] Result<Float64> read_double() {
        auto v = read_uint64();
        if (!v.is_success()) return make_error<Float64>(v.error().code, v.error().message);
        Float64 result;
        UInt64 raw = v.value();
        std::memcpy(&result, &raw, sizeof(result));
        return result;
    }
    
    // Read boolean
    [[nodiscard]] Result<bool> read_bool() {
        auto b = read_byte();
        if (!b.is_success()) return make_error<bool>(b.error().code, b.error().message);
        return b.value() != 0;
    }
    
    // Read string (length-prefixed)
    [[nodiscard]] Result<String> read_string() {
        auto len = read_uint32();
        if (!len.is_success()) return make_error<String>(len.error().code, len.error().message);
        
        if (remaining() < len.value()) {
            return make_error<String>(ErrorCode::ENDFILE, "Not enough data for string");
        }
        
        String result(reinterpret_cast<const char*>(data_.data() + position_), len.value());
        position_ += len.value();
        return result;
    }
    
    // Read fixed-length string
    [[nodiscard]] Result<String> read_fixed_string(Size length) {
        if (remaining() < length) {
            return make_error<String>(ErrorCode::ENDFILE, "Not enough data");
        }
        
        String result(reinterpret_cast<const char*>(data_.data() + position_), length);
        position_ += length;
        
        // Trim trailing nulls/spaces
        while (!result.empty() && (result.back() == '\0' || result.back() == ' ')) {
            result.pop_back();
        }
        return result;
    }
    
    // Read null-terminated string
    [[nodiscard]] Result<String> read_cstring() {
        Size start = position_;
        while (position_ < data_.size() && data_[position_] != 0) {
            ++position_;
        }
        
        String result(reinterpret_cast<const char*>(data_.data() + start), position_ - start);
        
        if (position_ < data_.size()) {
            ++position_;  // Skip null terminator
        }
        
        return result;
    }
};

// =============================================================================
// Serialization Interface
// =============================================================================

class ISerializable {
public:
    virtual ~ISerializable() = default;
    virtual void serialize(BinaryWriter& writer) const = 0;
    virtual Result<void> deserialize(BinaryReader& reader) = 0;
};

// =============================================================================
// Helper Functions
// =============================================================================

// Serialize to buffer
template<typename T>
[[nodiscard]] ByteBuffer serialize(const T& obj) requires requires { obj.serialize(std::declval<BinaryWriter&>()); } {
    BinaryWriter writer;
    obj.serialize(writer);
    return writer.take_buffer();
}

// Deserialize from buffer
template<typename T>
[[nodiscard]] Result<T> deserialize(ConstByteSpan data) requires std::is_default_constructible_v<T> {
    T obj;
    BinaryReader reader(data);
    auto result = obj.deserialize(reader);
    if (!result.is_success()) {
        return make_error<T>(result.error().code, result.error().message);
    }
    return obj;
}

} // namespace cics::serialization
