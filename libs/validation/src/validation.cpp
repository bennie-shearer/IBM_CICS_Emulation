// =============================================================================
// IBM CICS Emulation - Validation Utilities Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/validation/validation.hpp>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace cics::validation {

// =============================================================================
// ValidationError Implementation
// =============================================================================

String ValidationError::to_string() const {
    std::ostringstream oss;
    oss << field << ": " << message;
    if (!value.empty()) {
        oss << " (value: '" << value << "')";
    }
    return oss.str();
}

// =============================================================================
// ValidationResult Implementation
// =============================================================================

void ValidationResult::add_error(StringView field, StringView message, StringView value) {
    errors_.push_back(ValidationError{String(field), String(message), String(value)});
}

void ValidationResult::merge(const ValidationResult& other) {
    errors_.insert(errors_.end(), other.errors_.begin(), other.errors_.end());
}

String ValidationResult::to_string() const {
    if (errors_.empty()) {
        return "Validation passed";
    }
    
    std::ostringstream oss;
    oss << "Validation failed with " << errors_.size() << " error(s):\n";
    for (const auto& err : errors_) {
        oss << "  - " << err.to_string() << "\n";
    }
    return oss.str();
}

Vector<String> ValidationResult::error_messages() const {
    Vector<String> messages;
    messages.reserve(errors_.size());
    for (const auto& err : errors_) {
        messages.push_back(err.to_string());
    }
    return messages;
}

// =============================================================================
// String Validators
// =============================================================================

bool is_empty(StringView str) {
    return str.empty();
}

bool is_blank(StringView str) {
    return std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return std::isspace(c); });
}

bool has_min_length(StringView str, Size min_len) {
    return str.size() >= min_len;
}

bool has_max_length(StringView str, Size max_len) {
    return str.size() <= max_len;
}

bool has_length(StringView str, Size exact_len) {
    return str.size() == exact_len;
}

bool has_length_between(StringView str, Size min_len, Size max_len) {
    return str.size() >= min_len && str.size() <= max_len;
}

bool is_alpha(StringView str) {
    if (str.empty()) return false;
    return std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return std::isalpha(c); });
}

bool is_numeric(StringView str) {
    if (str.empty()) return false;
    return std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return std::isdigit(c); });
}

bool is_alphanumeric(StringView str) {
    if (str.empty()) return false;
    return std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return std::isalnum(c); });
}

bool is_ascii(StringView str) {
    return std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return c <= 127; });
}

bool is_printable(StringView str) {
    return std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return std::isprint(c) || std::isspace(c); });
}

bool is_uppercase(StringView str) {
    if (str.empty()) return false;
    return std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return !std::isalpha(c) || std::isupper(c); });
}

bool is_lowercase(StringView str) {
    if (str.empty()) return false;
    return std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return !std::isalpha(c) || std::islower(c); });
}

bool is_hex(StringView str) {
    if (str.empty()) return false;
    return std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return std::isxdigit(c); });
}

bool matches_pattern(StringView str, StringView pattern) {
    try {
        std::regex re(pattern.data(), pattern.size());
        return std::regex_match(str.begin(), str.end(), re);
    } catch (...) {
        return false;
    }
}

bool contains_only(StringView str, StringView allowed_chars) {
    return std::all_of(str.begin(), str.end(), [&allowed_chars](char c) {
        return allowed_chars.find(c) != StringView::npos;
    });
}

bool starts_with(StringView str, StringView prefix) {
    if (prefix.size() > str.size()) return false;
    return str.substr(0, prefix.size()) == prefix;
}

bool ends_with(StringView str, StringView suffix) {
    if (suffix.size() > str.size()) return false;
    return str.substr(str.size() - suffix.size()) == suffix;
}

// =============================================================================
// Number Validators
// =============================================================================

bool is_integer(StringView str) {
    if (str.empty()) return false;
    size_t start = 0;
    if (str[0] == '-' || str[0] == '+') {
        if (str.size() == 1) return false;
        start = 1;
    }
    return std::all_of(str.begin() + start, str.end(), 
        [](unsigned char c) { return std::isdigit(c); });
}

bool is_positive_integer(StringView str) {
    if (str.empty()) return false;
    if (str[0] == '-') return false;
    size_t start = (str[0] == '+') ? 1 : 0;
    if (start >= str.size()) return false;
    return std::all_of(str.begin() + start, str.end(), 
        [](unsigned char c) { return std::isdigit(c); });
}

bool is_negative_integer(StringView str) {
    if (str.empty() || str[0] != '-') return false;
    if (str.size() == 1) return false;
    return std::all_of(str.begin() + 1, str.end(), 
        [](unsigned char c) { return std::isdigit(c); });
}

bool is_decimal(StringView str) {
    if (str.empty()) return false;
    
    size_t start = 0;
    if (str[0] == '-' || str[0] == '+') {
        start = 1;
    }
    
    bool has_dot = false;
    bool has_digit = false;
    
    for (size_t i = start; i < str.size(); ++i) {
        if (str[i] == '.') {
            if (has_dot) return false;
            has_dot = true;
        } else if (std::isdigit(static_cast<unsigned char>(str[i]))) {
            has_digit = true;
        } else {
            return false;
        }
    }
    
    return has_digit;
}

bool is_number(StringView str) {
    return is_integer(str) || is_decimal(str);
}

// =============================================================================
// Format Validators
// =============================================================================

bool is_email(StringView str) {
    // Basic email validation
    if (str.empty()) return false;
    
    auto at_pos = str.find('@');
    if (at_pos == StringView::npos || at_pos == 0) return false;
    
    auto dot_pos = str.rfind('.');
    if (dot_pos == StringView::npos || dot_pos < at_pos + 2) return false;
    if (dot_pos == str.size() - 1) return false;
    
    return true;
}

bool is_url(StringView str) {
    // Basic URL validation
    return starts_with(str, "http://") || 
           starts_with(str, "https://") ||
           starts_with(str, "ftp://");
}

bool is_ipv4(StringView str) {
    // IPv4: xxx.xxx.xxx.xxx
    int dots = 0;
    int current_num = 0;
    bool has_digit = false;
    
    for (char c : str) {
        if (c == '.') {
            if (!has_digit || current_num > 255) return false;
            dots++;
            current_num = 0;
            has_digit = false;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            current_num = current_num * 10 + (c - '0');
            has_digit = true;
        } else {
            return false;
        }
    }
    
    return dots == 3 && has_digit && current_num <= 255;
}

bool is_ipv6(StringView str) {
    // Simplified IPv6 validation
    int colons = 0;
    int groups = 0;
    int group_len = 0;
    bool double_colon = false;
    
    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];
        if (c == ':') {
            if (i > 0 && str[i-1] == ':') {
                if (double_colon) return false;
                double_colon = true;
            } else if (group_len > 0) {
                groups++;
                group_len = 0;
            }
            colons++;
        } else if (std::isxdigit(static_cast<unsigned char>(c))) {
            group_len++;
            if (group_len > 4) return false;
        } else {
            return false;
        }
    }
    
    if (group_len > 0) groups++;
    
    if (double_colon) {
        return groups <= 8 && colons >= 2;
    }
    return groups == 8 && colons == 7;
}

bool is_ip_address(StringView str) {
    return is_ipv4(str) || is_ipv6(str);
}

bool is_date(StringView str, StringView format) {
    // Simple date validation for YYYY-MM-DD
    if (format == "YYYY-MM-DD" || format.empty()) {
        if (str.size() != 10) return false;
        if (str[4] != '-' || str[7] != '-') return false;
        
        auto year = str.substr(0, 4);
        auto month = str.substr(5, 2);
        auto day = str.substr(8, 2);
        
        if (!is_numeric(year) || !is_numeric(month) || !is_numeric(day)) {
            return false;
        }
        
        int m = std::stoi(String(month));
        int d = std::stoi(String(day));
        
        return m >= 1 && m <= 12 && d >= 1 && d <= 31;
    }
    
    return false;
}

bool is_time(StringView str, StringView format) {
    // Simple time validation for HH:MM:SS
    if (format == "HH:MM:SS" || format.empty()) {
        if (str.size() != 8) return false;
        if (str[2] != ':' || str[5] != ':') return false;
        
        auto hour = str.substr(0, 2);
        auto min = str.substr(3, 2);
        auto sec = str.substr(6, 2);
        
        if (!is_numeric(hour) || !is_numeric(min) || !is_numeric(sec)) {
            return false;
        }
        
        int h = std::stoi(String(hour));
        int m = std::stoi(String(min));
        int s = std::stoi(String(sec));
        
        return h >= 0 && h <= 23 && m >= 0 && m <= 59 && s >= 0 && s <= 59;
    }
    
    return false;
}

bool is_datetime(StringView str) {
    // YYYY-MM-DD HH:MM:SS or YYYY-MM-DDTHH:MM:SS
    if (str.size() < 19) return false;
    if (str[10] != ' ' && str[10] != 'T') return false;
    
    return is_date(str.substr(0, 10)) && is_time(str.substr(11, 8));
}

bool is_iso8601(StringView str) {
    if (str.size() < 10) return false;
    return is_date(str.substr(0, 10)) && 
           (str.size() == 10 || str[10] == 'T' || str[10] == ' ');
}

// =============================================================================
// CICS-Specific Validators
// =============================================================================

bool is_valid_cics_name(StringView name) {
    if (name.empty() || name.size() > 8) return false;
    if (!std::isalpha(static_cast<unsigned char>(name[0]))) return false;
    
    return std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '@' || c == '#' || c == '$';
    });
}

bool is_valid_transaction_id(StringView tranid) {
    if (tranid.empty() || tranid.size() > 4) return false;
    return is_valid_cics_name(tranid);
}

bool is_valid_program_name(StringView pgmname) {
    return is_valid_cics_name(pgmname);
}

bool is_valid_file_name(StringView filename) {
    return is_valid_cics_name(filename);
}

bool is_valid_dataset_name(StringView dsname) {
    if (dsname.empty() || dsname.size() > 44) return false;
    
    // Split by dots and validate each qualifier
    size_t start = 0;
    while (start < dsname.size()) {
        size_t dot_pos = dsname.find('.', start);
        if (dot_pos == StringView::npos) dot_pos = dsname.size();
        
        auto qualifier = dsname.substr(start, dot_pos - start);
        if (qualifier.empty() || qualifier.size() > 8) return false;
        if (!std::isalpha(static_cast<unsigned char>(qualifier[0]))) return false;
        
        if (!std::all_of(qualifier.begin(), qualifier.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '@' || c == '#' || c == '$';
        })) {
            return false;
        }
        
        start = dot_pos + 1;
    }
    
    return true;
}

bool is_valid_ebcdic(ConstByteSpan /* data */) {
    // All bytes are valid EBCDIC
    return true;  // EBCDIC is a 256-byte character set
}

bool is_valid_packed_decimal(ConstByteSpan data) {
    if (data.empty()) return false;
    
    // Check that last nibble is a valid sign (C, D, F for positive, negative, unsigned)
    Byte last_byte = data[data.size() - 1];
    Byte sign_nibble = last_byte & 0x0F;
    
    if (sign_nibble != 0x0C && sign_nibble != 0x0D && 
        sign_nibble != 0x0F && sign_nibble != 0x0A && sign_nibble != 0x0B) {
        return false;
    }
    
    // Check that all other nibbles are valid digits (0-9)
    for (size_t i = 0; i < data.size() - 1; ++i) {
        Byte high = (data[i] >> 4) & 0x0F;
        Byte low = data[i] & 0x0F;
        if (high > 9 || low > 9) return false;
    }
    
    // Check high nibble of last byte
    Byte last_high = (last_byte >> 4) & 0x0F;
    if (last_high > 9) return false;
    
    return true;
}

// =============================================================================
// Validator Builder Implementation
// =============================================================================

void Validator::check(bool condition, StringView message) {
    if (should_continue_ && !condition) {
        result_.add_error(current_field_, message, current_value_);
    }
}

Validator& Validator::field(StringView name, StringView value) {
    current_field_ = String(name);
    current_value_ = String(value);
    should_continue_ = true;
    return *this;
}

Validator& Validator::field(StringView name, Int64 value) {
    return field(name, std::to_string(value));
}

Validator& Validator::field(StringView name, Float64 value) {
    return field(name, std::to_string(value));
}

Validator& Validator::stop_on_error() {
    should_continue_ = !result_.has_errors();
    return *this;
}

Validator& Validator::required() {
    check(!is_empty(current_value_), "is required");
    return *this;
}

Validator& Validator::not_empty() {
    check(!is_blank(current_value_), "cannot be empty or blank");
    return *this;
}

Validator& Validator::min_length(Size len) {
    check(has_min_length(current_value_, len), 
          "must be at least " + std::to_string(len) + " characters");
    return *this;
}

Validator& Validator::max_length(Size len) {
    check(has_max_length(current_value_, len), 
          "must be at most " + std::to_string(len) + " characters");
    return *this;
}

Validator& Validator::length(Size len) {
    check(has_length(current_value_, len), 
          "must be exactly " + std::to_string(len) + " characters");
    return *this;
}

Validator& Validator::length_between(Size min_len, Size max_len) {
    check(has_length_between(current_value_, min_len, max_len), 
          "must be between " + std::to_string(min_len) + " and " + 
          std::to_string(max_len) + " characters");
    return *this;
}

Validator& Validator::alpha() {
    check(is_alpha(current_value_), "must contain only letters");
    return *this;
}

Validator& Validator::numeric() {
    check(is_numeric(current_value_), "must contain only digits");
    return *this;
}

Validator& Validator::alphanumeric() {
    check(is_alphanumeric(current_value_), "must be alphanumeric");
    return *this;
}

Validator& Validator::uppercase() {
    check(is_uppercase(current_value_), "must be uppercase");
    return *this;
}

Validator& Validator::lowercase() {
    check(is_lowercase(current_value_), "must be lowercase");
    return *this;
}

Validator& Validator::matches(StringView pattern) {
    check(matches_pattern(current_value_, pattern), 
          "must match pattern: " + String(pattern));
    return *this;
}

Validator& Validator::contains_only(StringView chars) {
    check(validation::contains_only(current_value_, chars), 
          "must contain only: " + String(chars));
    return *this;
}

Validator& Validator::positive() {
    check(is_positive_integer(current_value_) || 
          (is_decimal(current_value_) && current_value_[0] != '-'), 
          "must be positive");
    return *this;
}

Validator& Validator::non_negative() {
    check(!validation::starts_with(current_value_, "-"), "must be non-negative");
    return *this;
}

Validator& Validator::in_range(Int64 min_val, Int64 max_val) {
    if (is_integer(current_value_)) {
        Int64 val = std::stoll(current_value_);
        check(val >= min_val && val <= max_val, 
              "must be between " + std::to_string(min_val) + " and " + 
              std::to_string(max_val));
    } else {
        check(false, "must be a valid integer");
    }
    return *this;
}

Validator& Validator::in_range(Float64 min_val, Float64 max_val) {
    if (is_number(current_value_)) {
        Float64 val = std::stod(current_value_);
        check(val >= min_val && val <= max_val, 
              "must be between " + std::to_string(min_val) + " and " + 
              std::to_string(max_val));
    } else {
        check(false, "must be a valid number");
    }
    return *this;
}

Validator& Validator::greater_than(Int64 value) {
    if (is_integer(current_value_)) {
        Int64 val = std::stoll(current_value_);
        check(val > value, "must be greater than " + std::to_string(value));
    } else {
        check(false, "must be a valid integer");
    }
    return *this;
}

Validator& Validator::less_than(Int64 value) {
    if (is_integer(current_value_)) {
        Int64 val = std::stoll(current_value_);
        check(val < value, "must be less than " + std::to_string(value));
    } else {
        check(false, "must be a valid integer");
    }
    return *this;
}

Validator& Validator::email() {
    check(is_email(current_value_), "must be a valid email address");
    return *this;
}

Validator& Validator::url() {
    check(is_url(current_value_), "must be a valid URL");
    return *this;
}

Validator& Validator::ipv4() {
    check(is_ipv4(current_value_), "must be a valid IPv4 address");
    return *this;
}

Validator& Validator::date(StringView format) {
    check(is_date(current_value_, format), "must be a valid date");
    return *this;
}

Validator& Validator::time(StringView format) {
    check(is_time(current_value_, format), "must be a valid time");
    return *this;
}

Validator& Validator::cics_name() {
    check(is_valid_cics_name(current_value_), 
          "must be a valid CICS name (1-8 alphanumeric, starts with letter)");
    return *this;
}

Validator& Validator::transaction_id() {
    check(is_valid_transaction_id(current_value_), 
          "must be a valid transaction ID (1-4 characters)");
    return *this;
}

Validator& Validator::program_name() {
    check(is_valid_program_name(current_value_), 
          "must be a valid program name (1-8 characters)");
    return *this;
}

Validator& Validator::file_name() {
    check(is_valid_file_name(current_value_), 
          "must be a valid file name (1-8 characters)");
    return *this;
}

Validator& Validator::dataset_name() {
    check(is_valid_dataset_name(current_value_), 
          "must be a valid dataset name");
    return *this;
}

Validator& Validator::custom(std::function<bool(StringView)> predicate, StringView message) {
    check(predicate(current_value_), message);
    return *this;
}

ValidationResult Validator::validate() {
    return std::move(result_);
}

bool Validator::is_valid() {
    return result_.is_valid();
}

Result<void> Validator::to_result() {
    if (result_.is_valid()) {
        return {};
    }
    return make_error<void>(ErrorCode::INVALID_ARGUMENT, result_.to_string());
}

// =============================================================================
// Validation Functions
// =============================================================================

ValidationResult validate_cics_name(StringView name) {
    return Validator().field("name", name).cics_name().validate();
}

ValidationResult validate_transaction_id(StringView tranid) {
    return Validator().field("transaction_id", tranid).transaction_id().validate();
}

ValidationResult validate_program_name(StringView pgmname) {
    return Validator().field("program_name", pgmname).program_name().validate();
}

ValidationResult validate_dataset_name(StringView dsname) {
    return Validator().field("dataset_name", dsname).dataset_name().validate();
}

String sanitize_string(StringView str, Size max_len) {
    String result;
    result.reserve(str.size());
    
    for (char c : str) {
        if (std::isprint(static_cast<unsigned char>(c))) {
            result += c;
        }
    }
    
    if (max_len > 0 && result.size() > max_len) {
        result.resize(max_len);
    }
    
    return result;
}

String sanitize_name(StringView name, Size max_len) {
    String result;
    result.reserve(std::min(name.size(), max_len));
    
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || 
            c == '@' || c == '#' || c == '$') {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (result.size() >= max_len) break;
    }
    
    return result;
}

String sanitize_alphanumeric(StringView str) {
    String result;
    result.reserve(str.size());
    
    for (char c : str) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            result += c;
        }
    }
    
    return result;
}

// =============================================================================
// Assertion-Style Validation
// =============================================================================

namespace check {

void not_empty(StringView str, StringView field_name) {
    if (is_blank(str)) {
        throw std::invalid_argument(String(field_name) + " cannot be empty");
    }
}

void min_length(StringView str, Size min_len, StringView field_name) {
    if (!has_min_length(str, min_len)) {
        throw std::invalid_argument(String(field_name) + " must be at least " + 
                                    std::to_string(min_len) + " characters");
    }
}

void max_length(StringView str, Size max_len, StringView field_name) {
    if (!has_max_length(str, max_len)) {
        throw std::invalid_argument(String(field_name) + " must be at most " + 
                                    std::to_string(max_len) + " characters");
    }
}

void in_range(Int64 value, Int64 min_val, Int64 max_val, StringView field_name) {
    if (!is_in_range(value, min_val, max_val)) {
        throw std::out_of_range(String(field_name) + " must be between " + 
                                std::to_string(min_val) + " and " + 
                                std::to_string(max_val));
    }
}

void positive(Int64 value, StringView field_name) {
    if (!is_positive(value)) {
        throw std::invalid_argument(String(field_name) + " must be positive");
    }
}

void cics_name(StringView name, StringView field_name) {
    if (!is_valid_cics_name(name)) {
        throw std::invalid_argument(String(field_name) + 
            " must be a valid CICS name (1-8 alphanumeric, starts with letter)");
    }
}

void transaction_id(StringView tranid) {
    if (!is_valid_transaction_id(tranid)) {
        throw std::invalid_argument(
            "Transaction ID must be 1-4 alphanumeric characters");
    }
}

void program_name(StringView pgmname) {
    if (!is_valid_program_name(pgmname)) {
        throw std::invalid_argument(
            "Program name must be 1-8 alphanumeric characters");
    }
}

void file_name(StringView filename) {
    if (!is_valid_file_name(filename)) {
        throw std::invalid_argument(
            "File name must be 1-8 alphanumeric characters");
    }
}

} // namespace check

} // namespace cics::validation
