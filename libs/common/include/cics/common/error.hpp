#pragma once
// =============================================================================
// CICS Emulation - Error Handling (C++20)
// Version: 3.4.6
// =============================================================================

#include "cics/common/types.hpp"
#include <system_error>
#include <stdexcept>

namespace cics {

// =============================================================================
// Error Codes
// =============================================================================
enum class ErrorCode : Int32 {
    SUCCESS = 0,
    UNKNOWN_ERROR = 1000,
    INVALID_ARGUMENT = 1001,
    NULL_POINTER = 1002,
    OUT_OF_RANGE = 1003,
    BUFFER_OVERFLOW = 1004,
    INVALID_STATE = 1005,
    NOT_IMPLEMENTED = 1006,
    TIMEOUT = 1007,
    RESOURCE_EXHAUSTED = 1008,
    
    // I/O Errors (1100-1199)
    IO_ERROR = 1100,
    FILE_NOT_FOUND = 1101,
    FILE_EXISTS = 1102,
    PERMISSION_DENIED = 1103,
    DISK_FULL = 1104,
    READ_ERROR = 1105,
    WRITE_ERROR = 1106,
    
    // Memory Errors (1200-1299)
    OUT_OF_MEMORY = 1200,
    ALLOCATION_FAILED = 1201,
    MEMORY_CORRUPTION = 1202,
    
    // Security Errors (2000-2099)
    SECURITY_ERROR = 2000,
    AUTHENTICATION_FAILED = 2001,
    AUTHORIZATION_FAILED = 2002,
    INVALID_CREDENTIALS = 2003,
    SESSION_EXPIRED = 2004,
    ACCESS_DENIED = 2005,
    
    // Database/Catalog Errors (3000-3099)
    DATABASE_ERROR = 3000,
    RECORD_NOT_FOUND = 3001,
    DUPLICATE_KEY = 3002,
    DATASET_NOT_FOUND = 3003,
    CATALOG_ERROR = 3004,
    
    // VSAM Errors (4000-4099)
    VSAM_ERROR = 4000,
    VSAM_FILE_NOT_FOUND = 4001,
    VSAM_FILE_NOT_OPEN = 4002,
    VSAM_INVALID_REQUEST = 4003,
    VSAM_END_OF_FILE = 4004,
    VSAM_DUPLICATE_KEY = 4005,
    VSAM_RECORD_NOT_FOUND = 4006,
    VSAM_SEQUENCE_ERROR = 4007,
    VSAM_RBA_NOT_FOUND = 4008,
    VSAM_CI_FULL = 4009,
    VSAM_KEY_CHANGE = 4010,
    
    // CICS Errors (5000-5099)
    CICS_ERROR = 5000,
    CICS_ABEND = 5001,
    CICS_PROGRAM_NOT_FOUND = 5002,
    CICS_TRANSACTION_NOT_FOUND = 5003,
    CICS_FILE_NOT_FOUND = 5004,
    CICS_QUEUE_NOT_FOUND = 5005,
    CICS_TERMINAL_NOT_FOUND = 5006,
    
    // GDG Errors (6000-6099)
    GDG_ERROR = 6000,
    GDG_BASE_NOT_FOUND = 6001,
    GDG_GENERATION_NOT_FOUND = 6002,
    GDG_LIMIT_EXCEEDED = 6003,
    
    // HSM Errors (6100-6199)
    HSM_ERROR = 6100,
    HSM_MIGRATE_FAILED = 6101,
    HSM_RECALL_FAILED = 6102,
    
    // CICS Standard Condition Errors (7000-7099)
    ABEND = 7000,
    INVREQ = 7001,
    IOERR = 7002,
    LENGERR = 7003,
    NOTFND = 7004,
    QIDERR = 7005,
    ITEMERR = 7006,
    ENDFILE = 7007,
    NODATA = 7008,
    TIMEDOUT = 7009,
    TERMERR = 7010,
    TERMIDERR = 7011,
    CHANNELERR = 7012,
    CONTAINERERR = 7013,
    NOT_INITIALIZED = 7014,
    NOT_SUPPORTED = 7015,
    
    // Syncpoint Errors (7100-7149)
    SYNCPOINT_ERROR = 7100,
    UOW_NOT_FOUND = 7101,
    PREPARE_FAILED = 7102,
    COMMIT_FAILED = 7103,
    ROLLBACK_FAILED = 7104,
    
    // Spool Errors (7150-7199)
    SPOOL_ERROR = 7150,
    SPOOL_NOT_FOUND = 7151,
    SPOOL_NOT_OPEN = 7152
};

// =============================================================================
// Error Category
// =============================================================================
class CicsErrorCategory : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] String message(int code) const override;
    [[nodiscard]] bool equivalent(int code, const std::error_condition& condition) const noexcept override;
};

[[nodiscard]] const std::error_category& cics_error_category() noexcept;
[[nodiscard]] std::error_code make_error_code(ErrorCode e) noexcept;

} // namespace cics

namespace std {
    template<>
    struct is_error_code_enum<cics::ErrorCode> : true_type {};
}

namespace cics {

// =============================================================================
// ErrorInfo - Detailed error information
// =============================================================================
struct ErrorInfo {
    ErrorCode code = ErrorCode::SUCCESS;
    String message;
    String component;
    SystemTimePoint timestamp = SystemClock::now();
    std::source_location location = std::source_location::current();
    std::unordered_map<String, String> context;
    
    ErrorInfo() = default;
    ErrorInfo(ErrorCode c, String msg, String comp = "",
              std::source_location loc = std::source_location::current());
    
    ErrorInfo& with_context(String key, String value);
    
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_json() const;
    [[nodiscard]] String format_full() const;
};

// =============================================================================
// Result<T> - Monadic error handling
// =============================================================================
template<typename T>
class Result {
private:
    Variant<T, ErrorInfo> data_;
    
public:
    Result(T value) : data_(std::move(value)) {}
    Result(ErrorInfo error) : data_(std::move(error)) {}
    
    [[nodiscard]] bool is_success() const { return std::holds_alternative<T>(data_); }
    [[nodiscard]] bool is_error() const { return std::holds_alternative<ErrorInfo>(data_); }
    
    [[nodiscard]] T& value() & { return std::get<T>(data_); }
    [[nodiscard]] const T& value() const& { return std::get<T>(data_); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(data_)); }
    
    [[nodiscard]] ErrorInfo& error() & { return std::get<ErrorInfo>(data_); }
    [[nodiscard]] const ErrorInfo& error() const& { return std::get<ErrorInfo>(data_); }
    
    [[nodiscard]] T& operator*() & { return value(); }
    [[nodiscard]] const T& operator*() const& { return value(); }
    [[nodiscard]] T* operator->() { return &value(); }
    [[nodiscard]] const T* operator->() const { return &value(); }
    
    [[nodiscard]] T value_or(T default_val) const {
        return is_success() ? value() : std::move(default_val);
    }
    
    template<typename F>
    [[nodiscard]] auto map(F&& f) const -> Result<decltype(f(std::declval<T>()))> {
        if (is_success()) return f(value());
        return error();
    }
    
    template<typename F>
    [[nodiscard]] auto and_then(F&& f) const -> decltype(f(std::declval<T>())) {
        if (is_success()) return f(value());
        return error();
    }
    
    template<typename F>
    [[nodiscard]] Result<T> or_else(F&& f) const {
        if (is_success()) return *this;
        return f(error());
    }
    
    explicit operator bool() const { return is_success(); }
};

// Specialization for void
template<>
class Result<void> {
private:
    Optional<ErrorInfo> error_;
    
public:
    Result() = default;
    Result(ErrorInfo err) : error_(std::move(err)) {}
    
    [[nodiscard]] bool is_success() const { return !error_.has_value(); }
    [[nodiscard]] bool is_error() const { return error_.has_value(); }
    [[nodiscard]] const ErrorInfo& error() const { return *error_; }
    
    explicit operator bool() const { return is_success(); }
};

// =============================================================================
// Result Factory Functions
// =============================================================================
template<typename T>
[[nodiscard]] Result<T> make_success(T value) {
    return Result<T>(std::move(value));
}

[[nodiscard]] inline Result<void> make_success() {
    return Result<void>();
}

template<typename T>
[[nodiscard]] Result<T> make_error(ErrorCode code, String message,
                                   std::source_location loc = std::source_location::current()) {
    return Result<T>(ErrorInfo(code, std::move(message), "", loc));
}

template<typename T>
[[nodiscard]] Result<T> make_error(const ErrorInfo& info) {
    return Result<T>(info);
}

// =============================================================================
// Exception Hierarchy
// =============================================================================
class CicsException : public std::runtime_error {
protected:
    ErrorInfo error_info_;
    
public:
    explicit CicsException(ErrorInfo info);
    CicsException(ErrorCode code, const String& message,
                  std::source_location loc = std::source_location::current());
    
    [[nodiscard]] ErrorCode code() const { return error_info_.code; }
    [[nodiscard]] const ErrorInfo& error_info() const { return error_info_; }
    [[nodiscard]] String detailed_message() const;
};

class VsamException : public CicsException {
public:
    using CicsException::CicsException;
};

class CatalogException : public CicsException {
public:
    using CicsException::CicsException;
};

class SecurityException : public CicsException {
public:
    using CicsException::CicsException;
};

class TransactionException : public CicsException {
public:
    using CicsException::CicsException;
};

// =============================================================================
// Error Statistics
// =============================================================================
class ErrorStatistics {
private:
    std::unordered_map<ErrorCode, AtomicCounter<>> error_counts_;
    std::unordered_map<String, AtomicCounter<>> component_errors_;
    mutable std::shared_mutex mutex_;
    
public:
    static ErrorStatistics& instance();
    
    void record_error(const ErrorInfo& info);
    void reset();
    [[nodiscard]] UInt64 get_error_count(ErrorCode code) const;
    [[nodiscard]] UInt64 get_component_error_count(const String& component) const;
    [[nodiscard]] UInt64 total_errors() const;
};

// =============================================================================
// Helper Functions
// =============================================================================
[[nodiscard]] bool is_recoverable_error(ErrorCode code);
[[nodiscard]] StringView error_category_name(ErrorCode code);
[[nodiscard]] String format_error_code(ErrorCode code);

// =============================================================================
// Macros
// =============================================================================
#define CICS_TRY(expr) \
    do { \
        auto _result = (expr); \
        if (_result.is_error()) return _result.error(); \
    } while(0)

#define CICS_TRY_VOID(expr) \
    do { \
        auto _result = (expr); \
        if (_result.is_error()) return _result; \
    } while(0)

#define CICS_THROW_IF_ERROR(expr) \
    do { \
        auto _result = (expr); \
        if (_result.is_error()) throw cics::CicsException(_result.error()); \
    } while(0)

#define CICS_CHECK(cond, code, msg) \
    do { \
        if (!(cond)) return cics::make_error<decltype(this)>(code, msg); \
    } while(0)

} // namespace cics
