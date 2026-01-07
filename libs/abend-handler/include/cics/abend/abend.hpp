// =============================================================================
// CICS Emulation - Abend Handler
// =============================================================================
// Provides HANDLE ABEND, PUSH, POP functionality for exception handling.
// Implements abend codes and condition handling for transaction recovery.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#ifndef CICS_ABEND_HPP
#define CICS_ABEND_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <functional>
#include <vector>
#include <stack>
#include <memory>
#include <mutex>
#include <chrono>
#include <atomic>

namespace cics {
namespace abend {

// =============================================================================
// Forward Declarations
// =============================================================================

class AbendManager;
class AbendHandler;

// =============================================================================
// Standard CICS Abend Codes
// =============================================================================

namespace AbendCode {
    // System abends (Axxx)
    constexpr const char* ASRA = "ASRA";  // Program check
    constexpr const char* ASRB = "ASRB";  // Operating system abend
    constexpr const char* ASRD = "ASRD";  // External CICS interface error
    constexpr const char* AICA = "AICA";  // Runaway task
    constexpr const char* AICB = "AICB";  // CICS shut down
    constexpr const char* AICC = "AICC";  // Storage violation
    constexpr const char* AICD = "AICD";  // Storage shortage
    constexpr const char* AICE = "AICE";  // Transaction timeout
    constexpr const char* AKCS = "AKCS";  // Storage manager error
    constexpr const char* AKCT = "AKCT";  // Task control error
    
    // User abends (AExx, AFxx)
    constexpr const char* AEI0 = "AEI0";  // EXEC CICS error
    constexpr const char* AEI1 = "AEI1";  // Severe error
    constexpr const char* AEI2 = "AEI2";  // Recoverable error
    constexpr const char* AEI9 = "AEI9";  // User-requested abend
    
    // File control abends
    constexpr const char* AFCA = "AFCA";  // File not found
    constexpr const char* AFCB = "AFCB";  // File disabled
    constexpr const char* AFCC = "AFCC";  // I/O error
    constexpr const char* AFCD = "AFCD";  // Record not found
    constexpr const char* AFCE = "AFCE";  // Duplicate key
}

// =============================================================================
// Enumerations
// =============================================================================

enum class AbendAction {
    TERMINATE,      // Terminate the task
    HANDLE,         // Invoke the handler
    DUMP,           // Create a dump
    NODUMP          // No dump
};

enum class HandlerType {
    LABEL,          // Branch to a label (not applicable in C++)
    PROGRAM,        // Link to a program
    CANCEL,         // Cancel handling
    RESET           // Reset to default
};

// =============================================================================
// Abend Information
// =============================================================================

struct AbendInfo {
    FixedString<4> code;
    String message;
    String program;
    String transaction_id;
    UInt32 task_id;
    std::chrono::system_clock::time_point timestamp;
    bool dump_taken;
    String dump_id;
};

// =============================================================================
// Abend Handler Definition
// =============================================================================

using AbendCallback = std::function<void(const AbendInfo&)>;

struct HandlerDefinition {
    HandlerType type;
    String program_name;     // For PROGRAM type
    AbendCallback callback;  // For custom handling
    bool active = true;
};

// =============================================================================
// Handler Stack (for PUSH/POP)
// =============================================================================

class HandlerStack {
public:
    void push(const HandlerDefinition& handler);
    bool pop();
    [[nodiscard]] const HandlerDefinition* current() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] UInt32 depth() const;
    void clear();
    
private:
    std::stack<HandlerDefinition> stack_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Condition Handler
// =============================================================================

struct ConditionHandler {
    ErrorCode condition;
    HandlerDefinition handler;
};

// =============================================================================
// Abend Statistics
// =============================================================================

struct AbendStats {
    UInt64 abends_total{0};
    UInt64 abends_handled{0};
    UInt64 abends_terminated{0};
    UInt64 dumps_taken{0};
    UInt64 handlers_pushed{0};
    UInt64 handlers_popped{0};
    std::unordered_map<String, UInt64> abend_by_code;
};

// =============================================================================
// Abend Manager
// =============================================================================

class AbendManager {
public:
    static AbendManager& instance();
    
    // Lifecycle
    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }
    
    // Abend handling registration
    Result<void> handle_abend_program(StringView program);
    Result<void> handle_abend_cancel();
    Result<void> handle_abend_reset();
    Result<void> handle_abend_callback(AbendCallback callback);
    
    // Condition handling
    Result<void> handle_condition(ErrorCode condition, HandlerType type, StringView program = "");
    Result<void> ignore_condition(ErrorCode condition);
    
    // Handler stack operations
    Result<void> push_handler();
    Result<void> pop_handler();
    
    // Abend execution
    [[noreturn]] void abend(StringView code);
    [[noreturn]] void abend(StringView code, bool nodump);
    Result<void> abend_handled(StringView code);  // Non-fatal version
    
    // Information
    [[nodiscard]] AbendStats get_stats() const;
    [[nodiscard]] std::vector<AbendInfo> get_recent_abends(UInt32 count = 10) const;
    [[nodiscard]] const HandlerDefinition* current_handler() const;
    void reset_stats();
    
    // Dump control
    void set_dump_enabled(bool enabled) { dump_enabled_ = enabled; }
    [[nodiscard]] bool is_dump_enabled() const { return dump_enabled_; }
    void set_dump_directory(StringView dir) { dump_directory_ = String(dir); }
    [[nodiscard]] const String& dump_directory() const { return dump_directory_; }
    
    // Context
    void set_current_transaction(StringView transid) { current_transid_ = String(transid); }
    void set_current_program(StringView program) { current_program_ = String(program); }
    void set_current_task(UInt32 task_id) { current_task_id_ = task_id; }
    
private:
    AbendManager() = default;
    ~AbendManager() = default;
    AbendManager(const AbendManager&) = delete;
    AbendManager& operator=(const AbendManager&) = delete;
    
    void record_abend(const AbendInfo& info);
    String create_dump(const AbendInfo& info);
    void invoke_handler(const AbendInfo& info);
    
    bool initialized_ = false;
    bool dump_enabled_ = true;
    String dump_directory_ = "/tmp/cics_dumps";
    
    HandlerStack handler_stack_;
    HandlerDefinition default_handler_;
    std::unordered_map<ErrorCode, ConditionHandler> condition_handlers_;
    
    String current_transid_;
    String current_program_;
    UInt32 current_task_id_ = 0;
    
    std::vector<AbendInfo> recent_abends_;
    static constexpr UInt32 MAX_RECENT_ABENDS = 100;
    
    AbendStats stats_;
    mutable std::mutex mutex_;
};

// =============================================================================
// RAII Abend Handler Guard
// =============================================================================

class AbendHandlerGuard {
public:
    explicit AbendHandlerGuard(AbendCallback callback);
    explicit AbendHandlerGuard(StringView program);
    ~AbendHandlerGuard();
    
    AbendHandlerGuard(const AbendHandlerGuard&) = delete;
    AbendHandlerGuard& operator=(const AbendHandlerGuard&) = delete;
    
private:
    bool pushed_ = false;
};

// =============================================================================
// Abend Exception (for C++ exception handling integration)
// =============================================================================

class AbendException : public std::exception {
public:
    explicit AbendException(StringView code, StringView message = "");
    
    [[nodiscard]] const char* what() const noexcept override;
    [[nodiscard]] const FixedString<4>& code() const { return code_; }
    [[nodiscard]] const String& message() const { return message_; }
    [[nodiscard]] const AbendInfo& info() const { return info_; }
    
private:
    FixedString<4> code_;
    String message_;
    AbendInfo info_;
    mutable String what_string_;
};

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

Result<void> exec_cics_handle_abend_program(StringView program);
Result<void> exec_cics_handle_abend_label();  // Not applicable in C++
Result<void> exec_cics_handle_abend_cancel();
Result<void> exec_cics_handle_abend_reset();
Result<void> exec_cics_push_handle();
Result<void> exec_cics_pop_handle();
[[noreturn]] void exec_cics_abend(StringView code);
[[noreturn]] void exec_cics_abend_nodump(StringView code);

Result<void> exec_cics_handle_condition(ErrorCode condition, StringView program);
Result<void> exec_cics_ignore_condition(ErrorCode condition);

// =============================================================================
// Utility Functions
// =============================================================================

[[nodiscard]] String abend_code_description(StringView code);
[[nodiscard]] bool is_system_abend(StringView code);
[[nodiscard]] bool is_user_abend(StringView code);

} // namespace abend
} // namespace cics

#endif // CICS_ABEND_HPP
