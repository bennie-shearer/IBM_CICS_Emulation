// =============================================================================
// IBM CICS Emulation - String Utilities Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/stringutils/string_utils.hpp>

namespace cics::stringutils {

// =============================================================================
// Natural Sort Comparison
// =============================================================================

int compare_natural(StringView a, StringView b) {
    size_t i = 0, j = 0;
    
    while (i < a.size() && j < b.size()) {
        // Check if both characters are digits
        bool a_digit = std::isdigit(static_cast<unsigned char>(a[i]));
        bool b_digit = std::isdigit(static_cast<unsigned char>(b[j]));
        
        if (a_digit && b_digit) {
            // Compare numbers
            UInt64 num_a = 0, num_b = 0;
            
            while (i < a.size() && std::isdigit(static_cast<unsigned char>(a[i]))) {
                num_a = num_a * 10 + (a[i] - '0');
                ++i;
            }
            while (j < b.size() && std::isdigit(static_cast<unsigned char>(b[j]))) {
                num_b = num_b * 10 + (b[j] - '0');
                ++j;
            }
            
            if (num_a < num_b) return -1;
            if (num_a > num_b) return 1;
        } else {
            // Compare characters (case-insensitive)
            char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
            char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[j])));
            
            if (ca < cb) return -1;
            if (ca > cb) return 1;
            
            ++i;
            ++j;
        }
    }
    
    // Handle different lengths
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}

} // namespace cics::stringutils
