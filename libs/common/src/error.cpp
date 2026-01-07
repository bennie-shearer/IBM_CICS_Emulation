#include "cics/common/error.hpp"
#include <sstream>
#include <iomanip>

namespace cics {

const char* CicsErrorCategory::name() const noexcept { return "cics"; }

String CicsErrorCategory::message(int code) const {
    switch (static_cast<ErrorCode>(code)) {
        case ErrorCode::SUCCESS: return "Success";
        case ErrorCode::UNKNOWN_ERROR: return "Unknown error";
        case ErrorCode::INVALID_ARGUMENT: return "Invalid argument";
        case ErrorCode::NULL_POINTER: return "Null pointer";
        case ErrorCode::OUT_OF_RANGE: return "Out of range";
        case ErrorCode::BUFFER_OVERFLOW: return "Buffer overflow";
        case ErrorCode::INVALID_STATE: return "Invalid state";
        case ErrorCode::NOT_IMPLEMENTED: return "Not implemented";
        case ErrorCode::TIMEOUT: return "Operation timed out";
        case ErrorCode::IO_ERROR: return "I/O error";
        case ErrorCode::FILE_NOT_FOUND: return "File not found";
        case ErrorCode::PERMISSION_DENIED: return "Permission denied";
        case ErrorCode::OUT_OF_MEMORY: return "Out of memory";
        case ErrorCode::SECURITY_ERROR: return "Security error";
        case ErrorCode::AUTHENTICATION_FAILED: return "Authentication failed";
        case ErrorCode::AUTHORIZATION_FAILED: return "Authorization failed";
        case ErrorCode::DATABASE_ERROR: return "Database error";
        case ErrorCode::RECORD_NOT_FOUND: return "Record not found";
        case ErrorCode::DUPLICATE_KEY: return "Duplicate key";
        case ErrorCode::VSAM_ERROR: return "VSAM error";
        case ErrorCode::VSAM_FILE_NOT_FOUND: return "VSAM file not found";
        case ErrorCode::VSAM_END_OF_FILE: return "VSAM end of file";
        case ErrorCode::VSAM_DUPLICATE_KEY: return "VSAM duplicate key";
        case ErrorCode::VSAM_RECORD_NOT_FOUND: return "VSAM record not found";
        case ErrorCode::CICS_ERROR: return "CICS error";
        case ErrorCode::CICS_ABEND: return "CICS abend";
        case ErrorCode::CICS_PROGRAM_NOT_FOUND: return "Program not found";
        case ErrorCode::CICS_TRANSACTION_NOT_FOUND: return "Transaction not found";
        case ErrorCode::GDG_ERROR: return "GDG error";
        case ErrorCode::GDG_BASE_NOT_FOUND: return "GDG base not found";
        case ErrorCode::HSM_ERROR: return "HSM error";
        default: return "Unknown CICS error";
    }
}

bool CicsErrorCategory::equivalent(int code, const std::error_condition& condition) const noexcept {
    return default_error_condition(code) == condition;
}

const std::error_category& cics_error_category() noexcept {
    static CicsErrorCategory instance;
    return instance;
}

std::error_code make_error_code(ErrorCode e) noexcept {
    return {static_cast<int>(e), cics_error_category()};
}

ErrorInfo::ErrorInfo(ErrorCode c, String msg, String comp, std::source_location loc)
    : code(c), message(std::move(msg)), component(std::move(comp))
    , timestamp(SystemClock::now()), location(loc) {}

ErrorInfo& ErrorInfo::with_context(String key, String value) {
    context[std::move(key)] = std::move(value);
    return *this;
}

String ErrorInfo::to_string() const {
    return std::format("[{}] {}: {}", static_cast<int>(code), 
        cics_error_category().message(static_cast<int>(code)), message);
}

String ErrorInfo::to_json() const {
    std::ostringstream oss;
    oss << R"({"code":)" << static_cast<int>(code)
        << R"(,"message":")" << message << R"(")"
        << R"(,"component":")" << component << R"("})";
    return oss.str();
}

String ErrorInfo::format_full() const {
    std::ostringstream oss;
    oss << "Error: " << to_string() << "\n";
    oss << "  Component: " << (component.empty() ? "unknown" : component) << "\n";
    oss << "  Location: " << location.file_name() << ":" << location.line() << "\n";
    oss << "  Function: " << location.function_name() << "\n";
    if (!context.empty()) {
        oss << "  Context:\n";
        for (const auto& [k, v] : context) {
            oss << "    " << k << ": " << v << "\n";
        }
    }
    return oss.str();
}

CicsException::CicsException(ErrorInfo info)
    : std::runtime_error(info.to_string()), error_info_(std::move(info)) {}

CicsException::CicsException(ErrorCode code, const String& message, std::source_location loc)
    : std::runtime_error(message), error_info_(code, message, "", loc) {}

String CicsException::detailed_message() const {
    return error_info_.format_full();
}

ErrorStatistics& ErrorStatistics::instance() {
    static ErrorStatistics stats;
    return stats;
}

void ErrorStatistics::record_error(const ErrorInfo& info) {
    error_counts_[info.code]++;
    if (!info.component.empty()) {
        component_errors_[info.component]++;
    }
}

void ErrorStatistics::reset() {
    std::unique_lock lock(mutex_);
    error_counts_.clear();
    component_errors_.clear();
}

UInt64 ErrorStatistics::get_error_count(ErrorCode code) const {
    std::shared_lock lock(mutex_);
    auto it = error_counts_.find(code);
    return it != error_counts_.end() ? it->second.get() : 0;
}

UInt64 ErrorStatistics::get_component_error_count(const String& component) const {
    std::shared_lock lock(mutex_);
    auto it = component_errors_.find(component);
    return it != component_errors_.end() ? it->second.get() : 0;
}

UInt64 ErrorStatistics::total_errors() const {
    UInt64 total = 0;
    std::shared_lock lock(mutex_);
    for (const auto& [_, count] : error_counts_) {
        total += count.get();
    }
    return total;
}

bool is_recoverable_error(ErrorCode code) {
    switch (code) {
        case ErrorCode::TIMEOUT:
        case ErrorCode::RESOURCE_EXHAUSTED:
        case ErrorCode::VSAM_END_OF_FILE:
        case ErrorCode::RECORD_NOT_FOUND:
            return true;
        default:
            return false;
    }
}

StringView error_category_name(ErrorCode code) {
    int c = static_cast<int>(code);
    if (c >= 1000 && c < 2000) return "General";
    if (c >= 2000 && c < 3000) return "Security";
    if (c >= 3000 && c < 4000) return "Database";
    if (c >= 4000 && c < 5000) return "VSAM";
    if (c >= 5000 && c < 6000) return "CICS";
    if (c >= 6000 && c < 6100) return "GDG";
    if (c >= 6100 && c < 6200) return "HSM";
    return "Unknown";
}

String format_error_code(ErrorCode code) {
    return std::format("CICS{:04d}", static_cast<int>(code));
}

} // namespace cics
