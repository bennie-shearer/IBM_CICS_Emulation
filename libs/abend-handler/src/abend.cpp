// =============================================================================
// CICS Emulation - Abend Handler Implementation
// =============================================================================

#include <cics/abend/abend.hpp>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <ctime>

namespace cics {
namespace abend {

// =============================================================================
// Utility Functions
// =============================================================================

String abend_code_description(StringView code) {
    static const std::unordered_map<String, String> descriptions = {
        {"ASRA", "Program check exception"},
        {"ASRB", "Operating system abend"},
        {"ASRD", "External CICS interface error"},
        {"AICA", "Runaway task - infinite loop detected"},
        {"AICB", "CICS system shutdown in progress"},
        {"AICC", "Storage protection violation"},
        {"AICD", "Storage shortage - no storage available"},
        {"AICE", "Transaction timeout exceeded"},
        {"AKCS", "Storage manager error"},
        {"AKCT", "Task control error"},
        {"AEI0", "EXEC CICS command error"},
        {"AEI1", "Severe application error"},
        {"AEI2", "Recoverable application error"},
        {"AEI9", "User-requested abend"},
        {"AFCA", "File control - file not found"},
        {"AFCB", "File control - file disabled"},
        {"AFCC", "File control - I/O error"},
        {"AFCD", "File control - record not found"},
        {"AFCE", "File control - duplicate key"}
    };
    
    auto it = descriptions.find(String(code));
    if (it != descriptions.end()) {
        return it->second;
    }
    return "Unknown abend code";
}

bool is_system_abend(StringView code) {
    if (code.length() < 1) return false;
    char first = code[0];
    // System abends typically start with 'A' followed by letters
    return first == 'A' && code.length() >= 4;
}

bool is_user_abend(StringView code) {
    // User abends are 4-character codes, often numeric
    return code.length() == 4;
}

// =============================================================================
// HandlerStack Implementation
// =============================================================================

void HandlerStack::push(const HandlerDefinition& handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    stack_.push(handler);
}

bool HandlerStack::pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stack_.empty()) {
        return false;
    }
    stack_.pop();
    return true;
}

const HandlerDefinition* HandlerStack::current() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stack_.empty()) {
        return nullptr;
    }
    return &stack_.top();
}

bool HandlerStack::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stack_.empty();
}

UInt32 HandlerStack::depth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<UInt32>(stack_.size());
}

void HandlerStack::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!stack_.empty()) {
        stack_.pop();
    }
}

// =============================================================================
// AbendException Implementation
// =============================================================================

AbendException::AbendException(StringView code, StringView message)
    : code_(code), message_(message)
{
    info_.code = code_;
    info_.message = message_.empty() ? abend_code_description(code) : message_;
    info_.timestamp = std::chrono::system_clock::now();
    info_.dump_taken = false;
}

const char* AbendException::what() const noexcept {
    if (what_string_.empty()) {
        std::ostringstream oss;
        oss << "CICS Abend " << code_.str() << ": " << message_;
        what_string_ = oss.str();
    }
    return what_string_.c_str();
}

// =============================================================================
// AbendManager Implementation
// =============================================================================

AbendManager& AbendManager::instance() {
    static AbendManager instance;
    return instance;
}

void AbendManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;
    
    handler_stack_.clear();
    condition_handlers_.clear();
    recent_abends_.clear();
    
    // Set default handler
    default_handler_.type = HandlerType::CANCEL;
    default_handler_.active = true;
    
    reset_stats();
    initialized_ = true;
}

void AbendManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    handler_stack_.clear();
    condition_handlers_.clear();
    initialized_ = false;
}

Result<void> AbendManager::handle_abend_program(StringView program) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    HandlerDefinition handler;
    handler.type = HandlerType::PROGRAM;
    handler.program_name = String(program);
    handler.active = true;
    
    handler_stack_.push(handler);
    return make_success();
}

Result<void> AbendManager::handle_abend_cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    HandlerDefinition handler;
    handler.type = HandlerType::CANCEL;
    handler.active = true;
    
    handler_stack_.push(handler);
    return make_success();
}

Result<void> AbendManager::handle_abend_reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    HandlerDefinition handler;
    handler.type = HandlerType::RESET;
    handler.active = true;
    
    handler_stack_.push(handler);
    return make_success();
}

Result<void> AbendManager::handle_abend_callback(AbendCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    HandlerDefinition handler;
    handler.type = HandlerType::PROGRAM;
    handler.callback = std::move(callback);
    handler.active = true;
    
    handler_stack_.push(handler);
    return make_success();
}

Result<void> AbendManager::handle_condition(ErrorCode condition, HandlerType type, StringView program) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ConditionHandler ch;
    ch.condition = condition;
    ch.handler.type = type;
    ch.handler.program_name = String(program);
    ch.handler.active = true;
    
    condition_handlers_[condition] = ch;
    return make_success();
}

Result<void> AbendManager::ignore_condition(ErrorCode condition) {
    std::lock_guard<std::mutex> lock(mutex_);
    condition_handlers_.erase(condition);
    return make_success();
}

Result<void> AbendManager::push_handler() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Push a copy of the current handler state
    const HandlerDefinition* current = handler_stack_.current();
    if (current) {
        handler_stack_.push(*current);
    } else {
        handler_stack_.push(default_handler_);
    }
    
    ++stats_.handlers_pushed;
    return make_success();
}

Result<void> AbendManager::pop_handler() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!handler_stack_.pop()) {
        return make_error<void>(ErrorCode::INVALID_STATE,
            "Handler stack is empty");
    }
    
    ++stats_.handlers_popped;
    return make_success();
}

void AbendManager::abend(StringView code) {
    abend(code, false);
}

void AbendManager::abend(StringView code, bool nodump) {
    AbendInfo info;
    info.code = code;
    info.message = abend_code_description(code);
    info.program = current_program_;
    info.transaction_id = current_transid_;
    info.task_id = current_task_id_;
    info.timestamp = std::chrono::system_clock::now();
    info.dump_taken = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.abends_total;
        ++stats_.abend_by_code[info.code.str()];
    }
    
    // Take dump if enabled
    if (dump_enabled_ && !nodump) {
        info.dump_id = create_dump(info);
        info.dump_taken = !info.dump_id.empty();
        if (info.dump_taken) {
            ++stats_.dumps_taken;
        }
    }
    
    record_abend(info);
    
    // Check for handler
    const HandlerDefinition* handler = current_handler();
    if (handler && handler->type != HandlerType::CANCEL) {
        ++stats_.abends_handled;
        invoke_handler(info);
        // If handler returns, re-throw
    }
    
    ++stats_.abends_terminated;
    throw AbendException(code, info.message);
}

Result<void> AbendManager::abend_handled(StringView code) {
    AbendInfo info;
    info.code = code;
    info.message = abend_code_description(code);
    info.program = current_program_;
    info.transaction_id = current_transid_;
    info.task_id = current_task_id_;
    info.timestamp = std::chrono::system_clock::now();
    info.dump_taken = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.abends_total;
        ++stats_.abend_by_code[info.code.str()];
    }
    
    record_abend(info);
    
    // Check for handler
    const HandlerDefinition* handler = current_handler();
    if (handler && handler->callback) {
        ++stats_.abends_handled;
        handler->callback(info);
        return make_success();
    }
    
    return make_error<void>(ErrorCode::ABEND,
        "Abend " + info.code.str() + ": " + info.message);
}

void AbendManager::record_abend(const AbendInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    recent_abends_.push_back(info);
    if (recent_abends_.size() > MAX_RECENT_ABENDS) {
        recent_abends_.erase(recent_abends_.begin());
    }
}

String AbendManager::create_dump(const AbendInfo& info) {
    // Generate dump filename
    auto time_t_now = std::chrono::system_clock::to_time_t(info.timestamp);
    std::ostringstream filename;
    filename << dump_directory_ << "/dump_" 
             << info.code.str() << "_"
             << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S")
             << ".txt";
    
    // Create dump file
    std::ofstream dump_file(filename.str());
    if (!dump_file) {
        return "";
    }
    
    dump_file << "===================================================================\n";
    dump_file << "                    CICS TRANSACTION DUMP\n";
    dump_file << "===================================================================\n\n";
    
    dump_file << "ABEND CODE:      " << info.code.str() << "\n";
    dump_file << "DESCRIPTION:     " << info.message << "\n";
    dump_file << "TRANSACTION:     " << info.transaction_id << "\n";
    dump_file << "PROGRAM:         " << info.program << "\n";
    dump_file << "TASK ID:         " << info.task_id << "\n";
    dump_file << "TIMESTAMP:       " << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") << "\n";
    dump_file << "\n";
    
    dump_file << "-------------------------------------------------------------------\n";
    dump_file << "HANDLER INFORMATION\n";
    dump_file << "-------------------------------------------------------------------\n";
    dump_file << "Handler Stack Depth: " << handler_stack_.depth() << "\n";
    
    const HandlerDefinition* handler = current_handler();
    if (handler) {
        dump_file << "Current Handler Type: ";
        switch (handler->type) {
            case HandlerType::LABEL: dump_file << "LABEL\n"; break;
            case HandlerType::PROGRAM: dump_file << "PROGRAM (" << handler->program_name << ")\n"; break;
            case HandlerType::CANCEL: dump_file << "CANCEL\n"; break;
            case HandlerType::RESET: dump_file << "RESET\n"; break;
        }
    } else {
        dump_file << "No handler active\n";
    }
    
    dump_file << "\n";
    dump_file << "===================================================================\n";
    dump_file << "                      END OF DUMP\n";
    dump_file << "===================================================================\n";
    
    dump_file.close();
    return filename.str();
}

void AbendManager::invoke_handler(const AbendInfo& info) {
    const HandlerDefinition* handler = current_handler();
    if (!handler) return;
    
    if (handler->callback) {
        handler->callback(info);
    }
    // For PROGRAM type, would normally LINK to the program
    // This is a simplified implementation
}

AbendStats AbendManager::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

std::vector<AbendInfo> AbendManager::get_recent_abends(UInt32 count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    UInt32 actual_count = std::min(count, static_cast<UInt32>(recent_abends_.size()));
    std::vector<AbendInfo> result;
    result.reserve(actual_count);
    
    auto start = recent_abends_.end() - actual_count;
    for (auto it = start; it != recent_abends_.end(); ++it) {
        result.push_back(*it);
    }
    
    return result;
}

const HandlerDefinition* AbendManager::current_handler() const {
    return handler_stack_.current();
}

void AbendManager::reset_stats() {
    stats_.abends_total = 0;
    stats_.abends_handled = 0;
    stats_.abends_terminated = 0;
    stats_.dumps_taken = 0;
    stats_.handlers_pushed = 0;
    stats_.handlers_popped = 0;
    stats_.abend_by_code.clear();
}

// =============================================================================
// AbendHandlerGuard Implementation
// =============================================================================

AbendHandlerGuard::AbendHandlerGuard(AbendCallback callback) {
    auto result = AbendManager::instance().handle_abend_callback(std::move(callback));
    pushed_ = result.is_success();
}

AbendHandlerGuard::AbendHandlerGuard(StringView program) {
    auto result = AbendManager::instance().handle_abend_program(program);
    pushed_ = result.is_success();
}

AbendHandlerGuard::~AbendHandlerGuard() {
    if (pushed_) {
        AbendManager::instance().pop_handler();
    }
}

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

Result<void> exec_cics_handle_abend_program(StringView program) {
    return AbendManager::instance().handle_abend_program(program);
}

Result<void> exec_cics_handle_abend_label() {
    return make_error<void>(ErrorCode::NOT_SUPPORTED,
        "HANDLE ABEND LABEL is not supported in C++ - use callbacks instead");
}

Result<void> exec_cics_handle_abend_cancel() {
    return AbendManager::instance().handle_abend_cancel();
}

Result<void> exec_cics_handle_abend_reset() {
    return AbendManager::instance().handle_abend_reset();
}

Result<void> exec_cics_push_handle() {
    return AbendManager::instance().push_handler();
}

Result<void> exec_cics_pop_handle() {
    return AbendManager::instance().pop_handler();
}

void exec_cics_abend(StringView code) {
    AbendManager::instance().abend(code);
}

void exec_cics_abend_nodump(StringView code) {
    AbendManager::instance().abend(code, true);
}

Result<void> exec_cics_handle_condition(ErrorCode condition, StringView program) {
    return AbendManager::instance().handle_condition(condition, HandlerType::PROGRAM, program);
}

Result<void> exec_cics_ignore_condition(ErrorCode condition) {
    return AbendManager::instance().ignore_condition(condition);
}

} // namespace abend
} // namespace cics
