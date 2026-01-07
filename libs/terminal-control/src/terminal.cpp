// =============================================================================
// CICS Emulation - Terminal Control Implementation
// =============================================================================

#include <cics/terminal/terminal.hpp>
#include <algorithm>
#include <cstring>

namespace cics {
namespace terminal {

// Thread-local current terminal
thread_local String TerminalManager::current_terminal_id_;

// =============================================================================
// Utility Functions
// =============================================================================

String aid_key_to_string(AIDKey key) {
    switch (key) {
        case AIDKey::NONE: return "NONE";
        case AIDKey::ENTER: return "ENTER";
        case AIDKey::CLEAR: return "CLEAR";
        case AIDKey::PA1: return "PA1";
        case AIDKey::PA2: return "PA2";
        case AIDKey::PA3: return "PA3";
        case AIDKey::PF1: return "PF1";
        case AIDKey::PF2: return "PF2";
        case AIDKey::PF3: return "PF3";
        case AIDKey::PF4: return "PF4";
        case AIDKey::PF5: return "PF5";
        case AIDKey::PF6: return "PF6";
        case AIDKey::PF7: return "PF7";
        case AIDKey::PF8: return "PF8";
        case AIDKey::PF9: return "PF9";
        case AIDKey::PF10: return "PF10";
        case AIDKey::PF11: return "PF11";
        case AIDKey::PF12: return "PF12";
        case AIDKey::PF13: return "PF13";
        case AIDKey::PF14: return "PF14";
        case AIDKey::PF15: return "PF15";
        case AIDKey::PF16: return "PF16";
        case AIDKey::PF17: return "PF17";
        case AIDKey::PF18: return "PF18";
        case AIDKey::PF19: return "PF19";
        case AIDKey::PF20: return "PF20";
        case AIDKey::PF21: return "PF21";
        case AIDKey::PF22: return "PF22";
        case AIDKey::PF23: return "PF23";
        case AIDKey::PF24: return "PF24";
        default: return "UNKNOWN";
    }
}

AIDKey string_to_aid_key(StringView str) {
    static const std::unordered_map<String, AIDKey> key_map = {
        {"ENTER", AIDKey::ENTER}, {"CLEAR", AIDKey::CLEAR},
        {"PA1", AIDKey::PA1}, {"PA2", AIDKey::PA2}, {"PA3", AIDKey::PA3},
        {"PF1", AIDKey::PF1}, {"PF2", AIDKey::PF2}, {"PF3", AIDKey::PF3},
        {"PF4", AIDKey::PF4}, {"PF5", AIDKey::PF5}, {"PF6", AIDKey::PF6},
        {"PF7", AIDKey::PF7}, {"PF8", AIDKey::PF8}, {"PF9", AIDKey::PF9},
        {"PF10", AIDKey::PF10}, {"PF11", AIDKey::PF11}, {"PF12", AIDKey::PF12},
        {"PF13", AIDKey::PF13}, {"PF14", AIDKey::PF14}, {"PF15", AIDKey::PF15},
        {"PF16", AIDKey::PF16}, {"PF17", AIDKey::PF17}, {"PF18", AIDKey::PF18},
        {"PF19", AIDKey::PF19}, {"PF20", AIDKey::PF20}, {"PF21", AIDKey::PF21},
        {"PF22", AIDKey::PF22}, {"PF23", AIDKey::PF23}, {"PF24", AIDKey::PF24}
    };
    
    auto it = key_map.find(String(str));
    return (it != key_map.end()) ? it->second : AIDKey::NONE;
}

String terminal_type_to_string(TerminalType type) {
    switch (type) {
        case TerminalType::IBM_3270: return "IBM-3270";
        case TerminalType::IBM_3279: return "IBM-3279";
        case TerminalType::VT100: return "VT100";
        case TerminalType::CONSOLE: return "CONSOLE";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// TerminalSession Implementation
// =============================================================================

TerminalSession::TerminalSession(StringView terminal_id, TerminalType type)
    : terminal_id_(terminal_id)
    , type_(type)
{
    // Initialize screen buffer
    screen_buffer_.resize(static_cast<size_t>(rows_) * cols_, 0x00);
}

TerminalSession::~TerminalSession() {
    disconnect();
}

void TerminalSession::set_dimensions(UInt16 rows, UInt16 cols) {
    std::lock_guard<std::mutex> lock(mutex_);
    rows_ = rows;
    cols_ = cols;
    screen_buffer_.resize(static_cast<size_t>(rows_) * cols_, 0x00);
}

void TerminalSession::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = true;
    keyboard_locked_ = false;
}

void TerminalSession::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = false;
    keyboard_locked_ = true;
}

Result<void> TerminalSession::send_text(StringView text, const SendOptions& opts) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        return make_error<void>(ErrorCode::TERMERR, "Terminal not connected");
    }
    
    // Handle erase option
    if (opts.erase == EraseOption::ERASE) {
        clear_screen();
    } else if (opts.erase == EraseOption::ERASEAUP) {
        // Erase all unprotected - simplified to clear
        clear_screen();
    }
    
    // Handle cursor positioning
    if (opts.cursor == CursorOption::SET) {
        cursor_row_ = opts.cursor_row;
        cursor_col_ = opts.cursor_col;
    } else if (opts.cursor == CursorOption::HOME) {
        cursor_row_ = 1;
        cursor_col_ = 1;
    }
    
    // Write text to screen buffer
    write_to_screen(cursor_row_, cursor_col_, text);
    
    // Handle freekb
    if (opts.freekb) {
        keyboard_locked_ = false;
    }
    
    return make_success();
}

Result<void> TerminalSession::send_data(const ByteBuffer& data, const SendOptions& opts) {
    String text(reinterpret_cast<const char*>(data.data()), data.size());
    return send_text(text, opts);
}

Result<void> TerminalSession::send_control(const SendOptions& opts) {
    return send_text("", opts);
}

void TerminalSession::write_to_screen(UInt16 row, UInt16 col, StringView text) {
    UInt32 pos = static_cast<UInt32>((row - 1) * cols_ + (col - 1));
    
    for (char ch : text) {
        if (pos >= screen_buffer_.size()) break;
        
        if (ch == '\n') {
            // Move to next line
            pos = ((pos / cols_) + 1) * cols_;
            cursor_row_ = static_cast<UInt16>((pos / cols_) + 1);
            cursor_col_ = 1;
        } else {
            screen_buffer_[pos++] = static_cast<Byte>(ch);
            cursor_col_ = static_cast<UInt16>((pos % cols_) + 1);
            if (cursor_col_ == 1) {
                cursor_row_ = static_cast<UInt16>((pos / cols_) + 1);
            }
        }
    }
}

Result<TerminalInput> TerminalSession::receive(const ReceiveOptions& opts) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!connected_) {
        return make_error<TerminalInput>(ErrorCode::TERMERR, "Terminal not connected");
    }
    
    // Check for pending input
    if (input_queue_.empty()) {
        // No input available
        if (opts.timeout_ms == 0) {
            return make_error<TerminalInput>(ErrorCode::NODATA, "No input available");
        }
        // Would wait for input in real implementation
        return make_error<TerminalInput>(ErrorCode::TIMEDOUT, "Input timeout");
    }
    
    TerminalInput input = input_queue_.front();
    input_queue_.pop();
    last_aid_ = input.aid_key;
    
    return make_success(input);
}

Result<String> TerminalSession::receive_text(UInt32 max_length) {
    ReceiveOptions opts;
    opts.max_length = max_length;
    
    auto result = receive(opts);
    if (result.is_error()) {
        return make_error<String>(result.error().code, result.error().message);
    }
    
    const auto& data = result.value().data;
    UInt32 len = std::min(max_length, static_cast<UInt32>(data.size()));
    return make_success(String(reinterpret_cast<const char*>(data.data()), len));
}

void TerminalSession::set_cursor(UInt16 row, UInt16 col) {
    std::lock_guard<std::mutex> lock(mutex_);
    cursor_row_ = std::min(row, rows_);
    cursor_col_ = std::min(col, cols_);
}

std::pair<UInt16, UInt16> TerminalSession::get_cursor() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {cursor_row_, cursor_col_};
}

String TerminalSession::get_screen_text() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return String(reinterpret_cast<const char*>(screen_buffer_.data()), screen_buffer_.size());
}

String TerminalSession::get_screen_line(UInt16 row) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (row < 1 || row > rows_) {
        return "";
    }
    
    UInt32 start = static_cast<UInt32>((row - 1) * cols_);
    return String(reinterpret_cast<const char*>(screen_buffer_.data() + start), cols_);
}

void TerminalSession::clear_screen() {
    std::fill(screen_buffer_.begin(), screen_buffer_.end(), 0x00);
    cursor_row_ = 1;
    cursor_col_ = 1;
}

void TerminalSession::simulate_input(const TerminalInput& input) {
    std::lock_guard<std::mutex> lock(mutex_);
    input_queue_.push(input);
}

void TerminalSession::simulate_key(AIDKey key, StringView text) {
    TerminalInput input;
    input.aid_key = key;
    input.cursor_row = cursor_row_;
    input.cursor_col = cursor_col_;
    input.data.assign(text.begin(), text.end());
    input.received = std::chrono::steady_clock::now();
    
    simulate_input(input);
}

TerminalState TerminalSession::get_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    TerminalState state;
    state.terminal_id = terminal_id_;
    state.type = type_;
    state.rows = rows_;
    state.cols = cols_;
    state.cursor_row = cursor_row_;
    state.cursor_col = cursor_col_;
    state.connected = connected_;
    state.keyboard_locked = keyboard_locked_;
    state.last_aid = last_aid_;
    return state;
}

// =============================================================================
// TerminalManager Implementation
// =============================================================================

TerminalManager& TerminalManager::instance() {
    static TerminalManager instance;
    return instance;
}

void TerminalManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;
    
    sessions_.clear();
    reset_stats();
    initialized_ = true;
}

void TerminalManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [id, session] : sessions_) {
        session->disconnect();
    }
    
    sessions_.clear();
    current_terminal_id_.clear();
    initialized_ = false;
}

Result<TerminalSession*> TerminalManager::create_session(StringView terminal_id,
                                                          TerminalType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return make_error<TerminalSession*>(ErrorCode::NOT_INITIALIZED,
            "TerminalManager not initialized");
    }
    
    String key(terminal_id);
    auto it = sessions_.find(key);
    if (it != sessions_.end()) {
        return make_success(it->second.get());
    }
    
    auto session = std::make_unique<TerminalSession>(terminal_id, type);
    session->connect();
    
    TerminalSession* ptr = session.get();
    sessions_[key] = std::move(session);
    
    ++stats_.sessions_created;
    return make_success(ptr);
}

Result<TerminalSession*> TerminalManager::get_session(StringView terminal_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(String(terminal_id));
    if (it == sessions_.end()) {
        return make_error<TerminalSession*>(ErrorCode::TERMIDERR,
            "Terminal not found: " + String(terminal_id));
    }
    
    return make_success(it->second.get());
}

Result<void> TerminalManager::close_session(StringView terminal_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(String(terminal_id));
    if (it == sessions_.end()) {
        return make_error<void>(ErrorCode::TERMIDERR,
            "Terminal not found: " + String(terminal_id));
    }
    
    it->second->disconnect();
    sessions_.erase(it);
    
    ++stats_.sessions_closed;
    
    if (current_terminal_id_ == terminal_id) {
        current_terminal_id_.clear();
    }
    
    return make_success();
}

bool TerminalManager::has_session(StringView terminal_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.find(String(terminal_id)) != sessions_.end();
}

void TerminalManager::set_current_terminal(StringView terminal_id) {
    current_terminal_id_ = String(terminal_id);
}

TerminalSession* TerminalManager::current_terminal() {
    if (current_terminal_id_.empty()) {
        return nullptr;
    }
    
    auto result = get_session(current_terminal_id_);
    return result.is_success() ? result.value() : nullptr;
}

Result<void> TerminalManager::send(StringView text, const SendOptions& opts) {
    TerminalSession* session = current_terminal();
    if (!session) {
        return make_error<void>(ErrorCode::TERMERR, "No current terminal");
    }
    
    auto result = session->send_text(text, opts);
    if (result.is_success()) {
        ++stats_.sends_executed;
        stats_.bytes_sent += text.size();
        
        if (output_callback_) {
            ByteBuffer data(text.begin(), text.end());
            output_callback_(session->terminal_id(), data);
        }
    }
    return result;
}

Result<void> TerminalManager::send(const ByteBuffer& data, const SendOptions& opts) {
    TerminalSession* session = current_terminal();
    if (!session) {
        return make_error<void>(ErrorCode::TERMERR, "No current terminal");
    }
    
    auto result = session->send_data(data, opts);
    if (result.is_success()) {
        ++stats_.sends_executed;
        stats_.bytes_sent += data.size();
        
        if (output_callback_) {
            output_callback_(session->terminal_id(), data);
        }
    }
    return result;
}

// Suppress GCC 13 spurious warning with std::variant in Result type
#if defined(__GNUC__) && __GNUC__ >= 13
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif

Result<TerminalInput> TerminalManager::receive(const ReceiveOptions& opts) {
    TerminalSession* session = current_terminal();
    if (!session) {
        return make_error<TerminalInput>(ErrorCode::TERMERR, "No current terminal");
    }
    
    auto result = session->receive(opts);
    if (result.is_success()) {
        ++stats_.receives_executed;
        stats_.bytes_received += result.value().data.size();
    } else if (result.error().code == ErrorCode::TIMEDOUT) {
        ++stats_.timeouts;
    }
    return result;
}

#if defined(__GNUC__) && __GNUC__ >= 13
#pragma GCC diagnostic pop
#endif

Result<void> TerminalManager::control(const SendOptions& opts) {
    TerminalSession* session = current_terminal();
    if (!session) {
        return make_error<void>(ErrorCode::TERMERR, "No current terminal");
    }
    
    return session->send_control(opts);
}

std::vector<String> TerminalManager::list_terminals() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<String> ids;
    ids.reserve(sessions_.size());
    for (const auto& [id, session] : sessions_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<TerminalState> TerminalManager::list_terminal_states() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<TerminalState> states;
    states.reserve(sessions_.size());
    for (const auto& [id, session] : sessions_) {
        states.push_back(session->get_state());
    }
    return states;
}

TerminalStats TerminalManager::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void TerminalManager::reset_stats() {
    stats_.sends_executed = 0;
    stats_.receives_executed = 0;
    stats_.bytes_sent = 0;
    stats_.bytes_received = 0;
    stats_.sessions_created = 0;
    stats_.sessions_closed = 0;
    stats_.timeouts = 0;
}

void TerminalManager::set_output_callback(OutputCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    output_callback_ = std::move(callback);
}

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

Result<void> exec_cics_send_text(StringView text) {
    return TerminalManager::instance().send(text);
}

Result<void> exec_cics_send_text(StringView text, const SendOptions& opts) {
    return TerminalManager::instance().send(text, opts);
}

Result<void> exec_cics_send_text_erase(StringView text) {
    SendOptions opts;
    opts.erase = EraseOption::ERASE;
    return TerminalManager::instance().send(text, opts);
}

Result<void> exec_cics_send_text_freekb(StringView text) {
    SendOptions opts;
    opts.freekb = true;
    return TerminalManager::instance().send(text, opts);
}

Result<void> exec_cics_send_text_alarm(StringView text) {
    SendOptions opts;
    opts.alarm = true;
    return TerminalManager::instance().send(text, opts);
}

Result<void> exec_cics_send(const ByteBuffer& data) {
    return TerminalManager::instance().send(data);
}

Result<void> exec_cics_send(const ByteBuffer& data, const SendOptions& opts) {
    return TerminalManager::instance().send(data, opts);
}

Result<void> exec_cics_send_control_erase() {
    SendOptions opts;
    opts.erase = EraseOption::ERASE;
    return TerminalManager::instance().control(opts);
}

Result<void> exec_cics_send_control_freekb() {
    SendOptions opts;
    opts.freekb = true;
    return TerminalManager::instance().control(opts);
}

Result<void> exec_cics_send_control_alarm() {
    SendOptions opts;
    opts.alarm = true;
    return TerminalManager::instance().control(opts);
}

Result<void> exec_cics_send_control_cursor(UInt16 row, UInt16 col) {
    SendOptions opts;
    opts.cursor = CursorOption::SET;
    opts.cursor_row = row;
    opts.cursor_col = col;
    return TerminalManager::instance().control(opts);
}

Result<TerminalInput> exec_cics_receive() {
    return TerminalManager::instance().receive();
}

Result<TerminalInput> exec_cics_receive(const ReceiveOptions& opts) {
    return TerminalManager::instance().receive(opts);
}

Result<String> exec_cics_receive_text() {
    auto result = exec_cics_receive();
    if (result.is_error()) {
        return make_error<String>(result.error().code, result.error().message);
    }
    
    const auto& data = result.value().data;
    return make_success(String(reinterpret_cast<const char*>(data.data()), data.size()));
}

Result<UInt32> exec_cics_receive_into(void* buffer, UInt32 max_length) {
    auto result = exec_cics_receive();
    if (result.is_error()) {
        return make_error<UInt32>(result.error().code, result.error().message);
    }
    
    const auto& data = result.value().data;
    UInt32 copy_len = std::min(max_length, static_cast<UInt32>(data.size()));
    if (copy_len > 0 && buffer) {
        std::memcpy(buffer, data.data(), copy_len);
    }
    return make_success(copy_len);
}

Result<TerminalInput> exec_cics_converse(StringView text) {
    auto send_result = exec_cics_send_text(text);
    if (send_result.is_error()) {
        return make_error<TerminalInput>(send_result.error().code, send_result.error().message);
    }
    
    return exec_cics_receive();
}

Result<TerminalInput> exec_cics_converse(const ByteBuffer& data) {
    auto send_result = exec_cics_send(data);
    if (send_result.is_error()) {
        return make_error<TerminalInput>(send_result.error().code, send_result.error().message);
    }
    
    return exec_cics_receive();
}

} // namespace terminal
} // namespace cics
