#pragma once
// =============================================================================
// IBM CICS Emulation - Data Compression Utilities
// Version: 3.4.6
// Cross-platform compression for Windows, Linux, and macOS
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <algorithm>

namespace cics::compression {

// =============================================================================
// Compression Statistics
// =============================================================================

struct CompressionStats {
    Size original_size = 0;
    Size compressed_size = 0;
    
    [[nodiscard]] double ratio() const {
        if (original_size == 0) return 1.0;
        return static_cast<double>(compressed_size) / static_cast<double>(original_size);
    }
    
    [[nodiscard]] double savings_percent() const {
        return (1.0 - ratio()) * 100.0;
    }
};

// =============================================================================
// Run-Length Encoding (RLE)
// =============================================================================

namespace rle {

// Compress data using RLE
// Format: [count][byte] for runs, [0][count][bytes...] for literals
[[nodiscard]] inline Result<ByteBuffer> compress(ConstByteSpan data) {
    if (data.empty()) {
        return ByteBuffer{};
    }
    
    ByteBuffer result;
    result.reserve(data.size());
    
    Size i = 0;
    while (i < data.size()) {
        // Count consecutive identical bytes
        Byte current = data[i];
        Size run_length = 1;
        
        while (i + run_length < data.size() && 
               data[i + run_length] == current && 
               run_length < 127) {
            ++run_length;
        }
        
        if (run_length >= 3) {
            // Encode as run
            result.push_back(static_cast<Byte>(run_length));
            result.push_back(current);
            i += run_length;
        } else {
            // Collect literals
            Size literal_start = i;
            Size literal_count = 0;
            
            while (i < data.size() && literal_count < 127) {
                // Check if a run starts here
                Size potential_run = 1;
                while (i + potential_run < data.size() && 
                       data[i + potential_run] == data[i] && 
                       potential_run < 3) {
                    ++potential_run;
                }
                
                if (potential_run >= 3 && literal_count > 0) {
                    break;  // End literals, start a run
                }
                
                ++i;
                ++literal_count;
                
                if (potential_run >= 3) {
                    break;
                }
            }
            
            // Encode literals
            result.push_back(0);  // Literal marker
            result.push_back(static_cast<Byte>(literal_count));
            for (Size j = 0; j < literal_count; ++j) {
                result.push_back(data[literal_start + j]);
            }
        }
    }
    
    return result;
}

// Decompress RLE data
[[nodiscard]] inline Result<ByteBuffer> decompress(ConstByteSpan data) {
    if (data.empty()) {
        return ByteBuffer{};
    }
    
    ByteBuffer result;
    
    Size i = 0;
    while (i < data.size()) {
        Byte marker = data[i++];
        
        if (marker == 0) {
            // Literal sequence
            if (i >= data.size()) {
                return make_error<ByteBuffer>(ErrorCode::INVREQ, "Truncated RLE data");
            }
            Size count = data[i++];
            
            if (i + count > data.size()) {
                return make_error<ByteBuffer>(ErrorCode::INVREQ, "Truncated RLE literals");
            }
            
            for (Size j = 0; j < count; ++j) {
                result.push_back(data[i++]);
            }
        } else {
            // Run
            if (i >= data.size()) {
                return make_error<ByteBuffer>(ErrorCode::INVREQ, "Truncated RLE run");
            }
            Byte value = data[i++];
            
            for (Size j = 0; j < marker; ++j) {
                result.push_back(value);
            }
        }
    }
    
    return result;
}

} // namespace rle

// =============================================================================
// LZ77-style Compression (Simple Implementation)
// =============================================================================

namespace lz77 {

// Window size for lookback
constexpr Size WINDOW_SIZE = 4096;
constexpr Size MIN_MATCH = 3;
constexpr Size MAX_MATCH = 258;

// Compress data using LZ77
[[nodiscard]] inline Result<ByteBuffer> compress(ConstByteSpan data) {
    if (data.empty()) {
        return ByteBuffer{};
    }
    
    ByteBuffer result;
    result.reserve(data.size());
    
    Size pos = 0;
    while (pos < data.size()) {
        Size best_offset = 0;
        Size best_length = 0;
        
        // Search for matches in the window
        Size window_start = (pos > WINDOW_SIZE) ? pos - WINDOW_SIZE : 0;
        
        for (Size search = window_start; search < pos; ++search) {
            Size match_len = 0;
            while (pos + match_len < data.size() && 
                   match_len < MAX_MATCH &&
                   data[search + match_len] == data[pos + match_len]) {
                ++match_len;
            }
            
            if (match_len >= MIN_MATCH && match_len > best_length) {
                best_offset = pos - search;
                best_length = match_len;
            }
        }
        
        if (best_length >= MIN_MATCH) {
            // Encode as back-reference
            // Format: [0xFF][offset_high][offset_low][length]
            result.push_back(0xFF);
            result.push_back(static_cast<Byte>((best_offset >> 8) & 0xFF));
            result.push_back(static_cast<Byte>(best_offset & 0xFF));
            result.push_back(static_cast<Byte>(best_length - MIN_MATCH));
            pos += best_length;
        } else {
            // Encode as literal
            if (data[pos] == 0xFF) {
                result.push_back(0xFF);
                result.push_back(0x00);
                result.push_back(0x00);
                result.push_back(data[pos]);
            } else {
                result.push_back(data[pos]);
            }
            ++pos;
        }
    }
    
    return result;
}

// Decompress LZ77 data
[[nodiscard]] inline Result<ByteBuffer> decompress(ConstByteSpan data) {
    if (data.empty()) {
        return ByteBuffer{};
    }
    
    ByteBuffer result;
    
    Size i = 0;
    while (i < data.size()) {
        if (data[i] == 0xFF) {
            if (i + 3 >= data.size()) {
                return make_error<ByteBuffer>(ErrorCode::INVREQ, "Truncated LZ77 data");
            }
            
            Size offset = (static_cast<Size>(data[i + 1]) << 8) | data[i + 2];
            
            if (offset == 0) {
                // Escaped literal 0xFF
                result.push_back(data[i + 3]);
                i += 4;
            } else {
                // Back-reference
                Size length = static_cast<Size>(data[i + 3]) + MIN_MATCH;
                
                if (offset > result.size()) {
                    return make_error<ByteBuffer>(ErrorCode::INVREQ, "Invalid LZ77 offset");
                }
                
                Size src = result.size() - offset;
                for (Size j = 0; j < length; ++j) {
                    result.push_back(result[src + j]);
                }
                i += 4;
            }
        } else {
            result.push_back(data[i]);
            ++i;
        }
    }
    
    return result;
}

} // namespace lz77

// =============================================================================
// Huffman Coding (Static)
// =============================================================================

namespace huffman {

// Simple byte frequency table
struct FrequencyTable {
    std::array<Size, 256> freq{};
    
    void count(ConstByteSpan data) {
        for (Byte b : data) {
            ++freq[b];
        }
    }
    
    void reset() {
        freq.fill(0);
    }
};

} // namespace huffman

// =============================================================================
// Utility Functions
// =============================================================================

// Calculate compression statistics
[[nodiscard]] inline CompressionStats calculate_stats(Size original, Size compressed) {
    return CompressionStats{original, compressed};
}

// Simple compression using best method
[[nodiscard]] inline Result<ByteBuffer> compress(ConstByteSpan data) {
    // Try RLE first for simple data
    auto rle_result = rle::compress(data);
    if (!rle_result.is_success()) {
        return rle_result;
    }
    
    // If RLE made it smaller, use it
    if (rle_result.value().size() < data.size()) {
        ByteBuffer result;
        result.push_back(0x01);  // Method marker: RLE
        result.insert(result.end(), rle_result.value().begin(), rle_result.value().end());
        return result;
    }
    
    // Try LZ77
    auto lz_result = lz77::compress(data);
    if (!lz_result.is_success()) {
        return lz_result;
    }
    
    if (lz_result.value().size() < data.size()) {
        ByteBuffer result;
        result.push_back(0x02);  // Method marker: LZ77
        result.insert(result.end(), lz_result.value().begin(), lz_result.value().end());
        return result;
    }
    
    // No compression beneficial, store raw
    ByteBuffer result;
    result.push_back(0x00);  // Method marker: Raw
    result.insert(result.end(), data.begin(), data.end());
    return result;
}

// Decompress data (auto-detects method)
[[nodiscard]] inline Result<ByteBuffer> decompress(ConstByteSpan data) {
    if (data.empty()) {
        return ByteBuffer{};
    }
    
    Byte method = data[0];
    ConstByteSpan payload(data.data() + 1, data.size() - 1);
    
    switch (method) {
        case 0x00:  // Raw
            return ByteBuffer(payload.begin(), payload.end());
        case 0x01:  // RLE
            return rle::decompress(payload);
        case 0x02:  // LZ77
            return lz77::decompress(payload);
        default:
            return make_error<ByteBuffer>(ErrorCode::INVREQ, "Unknown compression method");
    }
}

// =============================================================================
// Compressor Class
// =============================================================================

class Compressor {
public:
    enum class Method {
        AUTO,
        RAW,
        RLE,
        LZ77
    };
    
private:
    Method method_ = Method::AUTO;
    CompressionStats last_stats_{};
    
public:
    Compressor() = default;
    explicit Compressor(Method method) : method_(method) {}
    
    void set_method(Method method) { method_ = method; }
    [[nodiscard]] Method method() const { return method_; }
    [[nodiscard]] const CompressionStats& last_stats() const { return last_stats_; }
    
    [[nodiscard]] Result<ByteBuffer> compress(ConstByteSpan data) {
        switch (method_) {
            case Method::RAW: {
                ByteBuffer buf;
                buf.push_back(0x00);
                buf.insert(buf.end(), data.begin(), data.end());
                last_stats_ = calculate_stats(data.size(), buf.size());
                return buf;
            }
            case Method::RLE: {
                auto rle_result = rle::compress(data);
                if (!rle_result.is_success()) return rle_result;
                ByteBuffer buf;
                buf.push_back(0x01);
                buf.insert(buf.end(), rle_result.value().begin(), rle_result.value().end());
                last_stats_ = calculate_stats(data.size(), buf.size());
                return buf;
            }
            case Method::LZ77: {
                auto lz_result = lz77::compress(data);
                if (!lz_result.is_success()) return lz_result;
                ByteBuffer buf;
                buf.push_back(0x02);
                buf.insert(buf.end(), lz_result.value().begin(), lz_result.value().end());
                last_stats_ = calculate_stats(data.size(), buf.size());
                return buf;
            }
            case Method::AUTO:
            default: {
                auto result = compression::compress(data);
                if (result.is_success()) {
                    last_stats_ = calculate_stats(data.size(), result.value().size());
                }
                return result;
            }
        }
    }
    
    [[nodiscard]] Result<ByteBuffer> decompress(ConstByteSpan data) {
        return compression::decompress(data);
    }
};

} // namespace cics::compression
