#pragma once
// =============================================================================
// IBM CICS Emulation - Validation Utilities
// Version: 3.4.6
// Input validation and data verification for Windows, Linux, and macOS
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <regex>

namespace cics::validation {

// =============================================================================
// Validation Result
// =============================================================================

struct ValidationError {
    String field;
    String message;
    String value;
    
    [[nodiscard]] String to_string() const;
};

class ValidationResult {
private:
    Vector<ValidationError> errors_;
    
public:
    ValidationResult() = default;
    
    // Status
    [[nodiscard]] bool is_valid() const { return errors_.empty(); }
    [[nodiscard]] bool has_errors() const { return !errors_.empty(); }
    [[nodiscard]] Size error_count() const { return errors_.size(); }
    
    // Error access
    [[nodiscard]] const Vector<ValidationError>& errors() const { return errors_; }
    [[nodiscard]] const ValidationError& first_error() const { return errors_.front(); }
    
    // Add errors
    void add_error(StringView field, StringView message, StringView value = "");
    void merge(const ValidationResult& other);
    void clear() { errors_.clear(); }
    
    // Formatting
    [[nodiscard]] String to_string() const;
    [[nodiscard]] Vector<String> error_messages() const;
    
    // Implicit conversion
    explicit operator bool() const { return is_valid(); }
};

// =============================================================================
// String Validators
// =============================================================================

// Check if string is empty or whitespace only
[[nodiscard]] bool is_empty(StringView str);
[[nodiscard]] bool is_blank(StringView str);

// Length validation
[[nodiscard]] bool has_min_length(StringView str, Size min_len);
[[nodiscard]] bool has_max_length(StringView str, Size max_len);
[[nodiscard]] bool has_length(StringView str, Size exact_len);
[[nodiscard]] bool has_length_between(StringView str, Size min_len, Size max_len);

// Character content
[[nodiscard]] bool is_alpha(StringView str);
[[nodiscard]] bool is_numeric(StringView str);
[[nodiscard]] bool is_alphanumeric(StringView str);
[[nodiscard]] bool is_ascii(StringView str);
[[nodiscard]] bool is_printable(StringView str);
[[nodiscard]] bool is_uppercase(StringView str);
[[nodiscard]] bool is_lowercase(StringView str);
[[nodiscard]] bool is_hex(StringView str);

// Pattern matching
[[nodiscard]] bool matches_pattern(StringView str, StringView pattern);
[[nodiscard]] bool contains_only(StringView str, StringView allowed_chars);
[[nodiscard]] bool starts_with(StringView str, StringView prefix);
[[nodiscard]] bool ends_with(StringView str, StringView suffix);

// =============================================================================
// Number Validators
// =============================================================================

// Integer validation
[[nodiscard]] bool is_integer(StringView str);
[[nodiscard]] bool is_positive_integer(StringView str);
[[nodiscard]] bool is_negative_integer(StringView str);

// Floating point validation
[[nodiscard]] bool is_decimal(StringView str);
[[nodiscard]] bool is_number(StringView str);

// Range validation
template<typename T>
[[nodiscard]] bool is_in_range(T value, T min_val, T max_val) {
    return value >= min_val && value <= max_val;
}

template<typename T>
[[nodiscard]] bool is_positive(T value) {
    return value > T{0};
}

template<typename T>
[[nodiscard]] bool is_non_negative(T value) {
    return value >= T{0};
}

// =============================================================================
// Format Validators
// =============================================================================

// Email validation (basic)
[[nodiscard]] bool is_email(StringView str);

// URL validation (basic)
[[nodiscard]] bool is_url(StringView str);

// IP address validation
[[nodiscard]] bool is_ipv4(StringView str);
[[nodiscard]] bool is_ipv6(StringView str);
[[nodiscard]] bool is_ip_address(StringView str);

// Date/time format validation
[[nodiscard]] bool is_date(StringView str, StringView format = "YYYY-MM-DD");
[[nodiscard]] bool is_time(StringView str, StringView format = "HH:MM:SS");
[[nodiscard]] bool is_datetime(StringView str);
[[nodiscard]] bool is_iso8601(StringView str);

// =============================================================================
// CICS-Specific Validators
// =============================================================================

// CICS name validation (1-8 uppercase alphanumeric, starts with letter)
[[nodiscard]] bool is_valid_cics_name(StringView name);

// Transaction ID (1-4 characters)
[[nodiscard]] bool is_valid_transaction_id(StringView tranid);

// Program name (1-8 characters)
[[nodiscard]] bool is_valid_program_name(StringView pgmname);

// File name (1-8 characters)
[[nodiscard]] bool is_valid_file_name(StringView filename);

// Dataset name validation
[[nodiscard]] bool is_valid_dataset_name(StringView dsname);

// EBCDIC content validation
[[nodiscard]] bool is_valid_ebcdic(ConstByteSpan data);

// Packed decimal validation
[[nodiscard]] bool is_valid_packed_decimal(ConstByteSpan data);

// =============================================================================
// Validator Builder (Fluent API)
// =============================================================================

class Validator {
private:
    ValidationResult result_;
    String current_field_;
    String current_value_;
    bool should_continue_ = true;
    
    void check(bool condition, StringView message);
    
public:
    Validator() = default;
    
    // Field selection
    Validator& field(StringView name, StringView value);
    Validator& field(StringView name, Int64 value);
    Validator& field(StringView name, Float64 value);
    
    // Stop on first error for this field
    Validator& stop_on_error();
    
    // String validations
    Validator& required();
    Validator& not_empty();
    Validator& min_length(Size len);
    Validator& max_length(Size len);
    Validator& length(Size len);
    Validator& length_between(Size min_len, Size max_len);
    Validator& alpha();
    Validator& numeric();
    Validator& alphanumeric();
    Validator& uppercase();
    Validator& lowercase();
    Validator& matches(StringView pattern);
    Validator& contains_only(StringView chars);
    
    // Number validations
    Validator& positive();
    Validator& non_negative();
    Validator& in_range(Int64 min_val, Int64 max_val);
    Validator& in_range(Float64 min_val, Float64 max_val);
    Validator& greater_than(Int64 value);
    Validator& less_than(Int64 value);
    
    // Format validations
    Validator& email();
    Validator& url();
    Validator& ipv4();
    Validator& date(StringView format = "YYYY-MM-DD");
    Validator& time(StringView format = "HH:MM:SS");
    
    // CICS validations
    Validator& cics_name();
    Validator& transaction_id();
    Validator& program_name();
    Validator& file_name();
    Validator& dataset_name();
    
    // Custom validation
    Validator& custom(std::function<bool(StringView)> predicate, StringView message);
    
    // Get result
    [[nodiscard]] ValidationResult validate();
    [[nodiscard]] bool is_valid();
    [[nodiscard]] Result<void> to_result();
};

// =============================================================================
// Validation Functions
// =============================================================================

// Quick validation
[[nodiscard]] ValidationResult validate_cics_name(StringView name);
[[nodiscard]] ValidationResult validate_transaction_id(StringView tranid);
[[nodiscard]] ValidationResult validate_program_name(StringView pgmname);
[[nodiscard]] ValidationResult validate_dataset_name(StringView dsname);

// Sanitization (returns cleaned string)
[[nodiscard]] String sanitize_string(StringView str, Size max_len = 0);
[[nodiscard]] String sanitize_name(StringView name, Size max_len = 8);
[[nodiscard]] String sanitize_alphanumeric(StringView str);

// =============================================================================
// Assertion-Style Validation
// =============================================================================

namespace check {

// These throw on validation failure
void not_empty(StringView str, StringView field_name = "value");
void min_length(StringView str, Size min_len, StringView field_name = "value");
void max_length(StringView str, Size max_len, StringView field_name = "value");
void in_range(Int64 value, Int64 min_val, Int64 max_val, StringView field_name = "value");
void positive(Int64 value, StringView field_name = "value");
void cics_name(StringView name, StringView field_name = "name");
void transaction_id(StringView tranid);
void program_name(StringView pgmname);
void file_name(StringView filename);

} // namespace check

} // namespace cics::validation
