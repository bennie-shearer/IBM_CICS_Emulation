#pragma once
// =============================================================================
// CICS Emulation - Core Types (C++20)
// Version: 3.4.6
// =============================================================================

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <span>
#include <optional>
#include <variant>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <format>
#include <concepts>
#include <type_traits>
#include <filesystem>
#include <source_location>
#include <unordered_map>

namespace cics {

// =============================================================================
// Fundamental Types
// =============================================================================
using Byte = std::uint8_t;
using Int8 = std::int8_t;
using UInt8 = std::uint8_t;
using Int16 = std::int16_t;
using UInt16 = std::uint16_t;
using Int32 = std::int32_t;
using UInt32 = std::uint32_t;
using Int64 = std::int64_t;
using UInt64 = std::uint64_t;
using Float32 = float;
using Float64 = double;
using Size = std::size_t;

// =============================================================================
// String Types
// =============================================================================
using String = std::string;
using StringView = std::string_view;
using WString = std::wstring;
using WStringView = std::wstring_view;

// =============================================================================
// Container Types
// =============================================================================
using ByteBuffer = std::vector<Byte>;
using ByteSpan = std::span<Byte>;
using ConstByteSpan = std::span<const Byte>;
using EbcdicString = std::vector<Byte>;
template<typename T> using Vector = std::vector<T>;

// =============================================================================
// Smart Pointers
// =============================================================================
template<typename T> using UniquePtr = std::unique_ptr<T>;
template<typename T> using SharedPtr = std::shared_ptr<T>;
template<typename T> using WeakPtr = std::weak_ptr<T>;

template<typename T, typename... Args>
[[nodiscard]] UniquePtr<T> make_unique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
[[nodiscard]] SharedPtr<T> make_shared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// =============================================================================
// Optional and Variant
// =============================================================================
template<typename T> using Optional = std::optional<T>;
template<typename... Ts> using Variant = std::variant<Ts...>;
inline constexpr std::nullopt_t nullopt = std::nullopt;

// =============================================================================
// Time Types
// =============================================================================
using Clock = std::chrono::steady_clock;
using SystemClock = std::chrono::system_clock;
using TimePoint = Clock::time_point;
using SystemTimePoint = SystemClock::time_point;
using Duration = Clock::duration;
using Nanoseconds = std::chrono::nanoseconds;
using Microseconds = std::chrono::microseconds;
using Milliseconds = std::chrono::milliseconds;
using Seconds = std::chrono::seconds;
using Minutes = std::chrono::minutes;

// =============================================================================
// Filesystem
// =============================================================================
using Path = std::filesystem::path;

// =============================================================================
// C++20 Concepts
// =============================================================================
template<typename T>
concept Integral = std::is_integral_v<T>;

template<typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

template<typename T>
concept Numeric = Integral<T> || FloatingPoint<T>;

template<typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

template<typename T>
concept Serializable = requires(T a) {
    { a.serialize() } -> std::convertible_to<ByteBuffer>;
};

template<typename T>
concept Comparable = requires(T a, T b) {
    { a == b } -> std::convertible_to<bool>;
    { a != b } -> std::convertible_to<bool>;
    { a < b } -> std::convertible_to<bool>;
};

template<typename T>
concept Container = requires(T a) {
    typename T::value_type;
    { a.begin() };
    { a.end() };
    { a.size() } -> std::convertible_to<std::size_t>;
};

// =============================================================================
// FixedString - Mainframe-style fixed-length string
// =============================================================================
template<Size N>
class FixedString {
private:
    std::array<char, N> data_{};
    
public:
    constexpr FixedString() noexcept { data_.fill(' '); }
    
    constexpr FixedString(StringView sv) noexcept {
        data_.fill(' ');
        Size len = std::min(sv.size(), N);
        for (Size i = 0; i < len; ++i) data_[i] = sv[i];
    }
    
    [[nodiscard]] constexpr Size length() const noexcept { return N; }
    [[nodiscard]] constexpr Size size() const noexcept { return N; }
    [[nodiscard]] constexpr const char* data() const noexcept { return data_.data(); }
    [[nodiscard]] constexpr char* data() noexcept { return data_.data(); }
    
    [[nodiscard]] String str() const { return String(data_.data(), N); }
    
    [[nodiscard]] String trimmed() const {
        Size end = N;
        while (end > 0 && data_[end - 1] == ' ') --end;
        return String(data_.data(), end);
    }
    
    constexpr char& operator[](Size i) { return data_[i]; }
    constexpr const char& operator[](Size i) const { return data_[i]; }
    
    constexpr auto operator<=>(const FixedString&) const = default;
    constexpr bool operator==(const FixedString&) const = default;
    
    [[nodiscard]] constexpr bool empty() const noexcept {
        for (Size i = 0; i < N; ++i) if (data_[i] != ' ') return false;
        return true;
    }
    
    constexpr void clear() noexcept { data_.fill(' '); }
};

// =============================================================================
// BufferView - Zero-copy buffer viewing
// =============================================================================
class BufferView {
private:
    const Byte* data_ = nullptr;
    Size size_ = 0;
    
public:
    constexpr BufferView() noexcept = default;
    constexpr BufferView(const Byte* data, Size size) noexcept : data_(data), size_(size) {}
    explicit BufferView(const ByteBuffer& buf) noexcept : data_(buf.data()), size_(buf.size()) {}
    explicit BufferView(ConstByteSpan span) noexcept : data_(span.data()), size_(span.size()) {}
    
    [[nodiscard]] constexpr const Byte* data() const noexcept { return data_; }
    [[nodiscard]] constexpr Size size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    
    [[nodiscard]] constexpr Byte operator[](Size i) const { return data_[i]; }
    [[nodiscard]] constexpr const Byte* begin() const noexcept { return data_; }
    [[nodiscard]] constexpr const Byte* end() const noexcept { return data_ + size_; }
    
    [[nodiscard]] constexpr BufferView subview(Size offset, Size count) const {
        if (offset >= size_) return {};
        return BufferView(data_ + offset, std::min(count, size_ - offset));
    }
    
    [[nodiscard]] ConstByteSpan span() const noexcept { return {data_, size_}; }
};

// =============================================================================
// AtomicCounter - Thread-safe counter
// =============================================================================
template<typename T = UInt64>
class AtomicCounter {
private:
    std::atomic<T> value_{0};
    
public:
    AtomicCounter() = default;
    explicit AtomicCounter(T initial) : value_(initial) {}
    
    T operator++() noexcept { return value_.fetch_add(1, std::memory_order_relaxed) + 1; }
    T operator++(int) noexcept { return value_.fetch_add(1, std::memory_order_relaxed); }
    T operator--() noexcept { return value_.fetch_sub(1, std::memory_order_relaxed) - 1; }
    T operator--(int) noexcept { return value_.fetch_sub(1, std::memory_order_relaxed); }
    T operator+=(T v) noexcept { return value_.fetch_add(v, std::memory_order_relaxed) + v; }
    T operator-=(T v) noexcept { return value_.fetch_sub(v, std::memory_order_relaxed) - v; }
    
    [[nodiscard]] T get() const noexcept { return value_.load(std::memory_order_relaxed); }
    void set(T v) noexcept { value_.store(v, std::memory_order_relaxed); }
    void reset() noexcept { value_.store(0, std::memory_order_relaxed); }
    
    operator T() const noexcept { return get(); }
};

// =============================================================================
// UUID
// =============================================================================
struct UUID {
    std::array<Byte, 16> data{};
    
    static UUID generate();
    [[nodiscard]] String to_string() const;
    [[nodiscard]] bool is_nil() const;
    
    auto operator<=>(const UUID&) const = default;
};

// =============================================================================
// Version
// =============================================================================
struct Version {
    UInt16 major = 0;
    UInt16 minor = 0;
    UInt16 patch = 0;
    String pre_release;
    
    [[nodiscard]] String to_string() const {
        String s = std::format("{}.{}.{}", major, minor, patch);
        if (!pre_release.empty()) s += "-" + pre_release;
        return s;
    }
    
    auto operator<=>(const Version&) const = default;
    static Version parse(StringView sv);
};

// =============================================================================
// PerformanceMetrics
// =============================================================================
struct PerformanceMetrics {
    AtomicCounter<> total_operations;
    AtomicCounter<> successful_operations;
    AtomicCounter<> failed_operations;
    AtomicCounter<UInt64> total_bytes_processed;
    AtomicCounter<UInt64> total_response_time_ns;
    Duration min_response_time = Duration::max();
    Duration max_response_time = Duration::zero();
    TimePoint start_time = Clock::now();
    mutable std::mutex timing_mutex;
    
    void record_operation(Duration response_time, bool success, Size bytes = 0);
    void reset();
    [[nodiscard]] Duration average_response_time() const;
    [[nodiscard]] double operations_per_second() const;
    [[nodiscard]] double success_rate() const;
    [[nodiscard]] double throughput_mbps() const;
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_json() const;
};

// =============================================================================
// CapacityInfo
// =============================================================================
struct CapacityInfo {
    UInt64 total_bytes = 0;
    UInt64 used_bytes = 0;
    UInt64 free_bytes = 0;
    
    [[nodiscard]] double utilization_percent() const {
        return total_bytes > 0 ?
            (static_cast<double>(used_bytes) / static_cast<double>(total_bytes)) * 100.0 : 0.0;
    }
    [[nodiscard]] bool is_full() const { return free_bytes == 0; }
    [[nodiscard]] bool is_critical(double threshold = 90.0) const {
        return utilization_percent() >= threshold;
    }
};

// =============================================================================
// PackedDecimal - Mainframe packed decimal
// =============================================================================
class PackedDecimal {
private:
    ByteBuffer data_;
    UInt8 scale_ = 0;
    
public:
    PackedDecimal() = default;
    explicit PackedDecimal(const ByteBuffer& data, UInt8 scale = 0)
        : data_(data), scale_(scale) {}
    
    [[nodiscard]] String to_string() const;
    bool from_string(StringView str);
    [[nodiscard]] Int64 to_int64() const;
    [[nodiscard]] Float64 to_double() const;
    
    [[nodiscard]] bool is_positive() const;
    [[nodiscard]] bool is_negative() const;
    [[nodiscard]] bool is_zero() const;
    [[nodiscard]] PackedDecimal abs() const;
    [[nodiscard]] PackedDecimal negate() const;
    
    [[nodiscard]] const ByteBuffer& data() const { return data_; }
    [[nodiscard]] UInt8 scale() const { return scale_; }
    
    auto operator<=>(const PackedDecimal& other) const;
    bool operator==(const PackedDecimal& other) const;
};

// =============================================================================
// Lazy<T> - Thread-safe lazy initialization
// =============================================================================
template<typename T>
class Lazy {
private:
    mutable std::once_flag flag_;
    mutable Optional<T> value_;
    std::function<T()> factory_;
    
public:
    explicit Lazy(std::function<T()> factory) : factory_(std::move(factory)) {}
    
    const T& get() const {
        std::call_once(flag_, [this]() { value_.emplace(factory_()); });
        return *value_;
    }
    
    T& get() {
        std::call_once(flag_, [this]() { value_.emplace(factory_()); });
        return *value_;
    }
    
    const T& operator*() const { return get(); }
    T& operator*() { return get(); }
    const T* operator->() const { return &get(); }
    T* operator->() { return &get(); }
};

// =============================================================================
// String Utilities
// =============================================================================
[[nodiscard]] String to_upper(StringView str);
[[nodiscard]] String to_lower(StringView str);
[[nodiscard]] String trim(StringView str);
[[nodiscard]] String trim_left(StringView str);
[[nodiscard]] String trim_right(StringView str);
[[nodiscard]] std::vector<String> split(StringView str, char delimiter);
[[nodiscard]] std::vector<String> split(StringView str, StringView delimiter);
[[nodiscard]] String join(const std::vector<String>& strings, StringView delimiter);
[[nodiscard]] bool starts_with(StringView str, StringView prefix);
[[nodiscard]] bool ends_with(StringView str, StringView suffix);
[[nodiscard]] bool contains(StringView str, StringView substr);
[[nodiscard]] String replace_all(StringView str, StringView from, StringView to);
[[nodiscard]] String pad_left(StringView str, Size width, char pad = ' ');
[[nodiscard]] String pad_right(StringView str, Size width, char pad = ' ');

// =============================================================================
// EBCDIC Conversion
// =============================================================================
[[nodiscard]] EbcdicString ascii_to_ebcdic(StringView ascii);
[[nodiscard]] String ebcdic_to_ascii(ConstByteSpan ebcdic);
[[nodiscard]] bool is_valid_ebcdic(ConstByteSpan data);

// =============================================================================
// Hash and Encoding
// =============================================================================
[[nodiscard]] UInt32 crc32(ConstByteSpan data);
[[nodiscard]] UInt64 fnv1a_hash(ConstByteSpan data);
[[nodiscard]] String to_hex_string(ConstByteSpan data);
[[nodiscard]] ByteBuffer from_hex_string(StringView hex);

// =============================================================================
// Byte Order
// =============================================================================
[[nodiscard]] constexpr bool is_little_endian() noexcept {
    return std::endian::native == std::endian::little;
}

[[nodiscard]] constexpr bool is_big_endian() noexcept {
    return std::endian::native == std::endian::big;
}

template<Integral T>
[[nodiscard]] constexpr T byte_swap(T value) noexcept {
    if constexpr (sizeof(T) == 1) return value;
    else if constexpr (sizeof(T) == 2) return static_cast<T>(__builtin_bswap16(value));
    else if constexpr (sizeof(T) == 4) return static_cast<T>(__builtin_bswap32(value));
    else if constexpr (sizeof(T) == 8) return static_cast<T>(__builtin_bswap64(value));
}

template<Integral T>
[[nodiscard]] constexpr T host_to_big_endian(T value) noexcept {
    return is_little_endian() ? byte_swap(value) : value;
}

template<Integral T>
[[nodiscard]] constexpr T big_endian_to_host(T value) noexcept {
    return is_little_endian() ? byte_swap(value) : value;
}

} // namespace cics
