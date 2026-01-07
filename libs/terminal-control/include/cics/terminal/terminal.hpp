// =============================================================================
// CICS Emulation - Terminal Control
// =============================================================================
// Provides SEND TEXT, RECEIVE, SEND CONTROL functionality.
// Implements terminal I/O beyond BMS maps.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#ifndef CICS_TERMINAL_HPP
#define CICS_TERMINAL_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <functional>
#include <queue>
#include <memory>
#include <mutex>
#include <atomic>
#include <optional>

namespace cics {
namespace terminal {

// =============================================================================
// Constants
// =============================================================================

constexpr UInt16 DEFAULT_SCREEN_ROWS = 24;
constexpr UInt16 DEFAULT_SCREEN_COLS = 80;
constexpr UInt32 MAX_INPUT_LENGTH = 32767;

// =============================================================================
// Enumerations
// =============================================================================

enum class TerminalType {
    IBM_3270,       // IBM 3270 terminal
    IBM_3279,       // IBM 3279 color terminal
    VT100,          // VT100 emulation
    CONSOLE         // Console/telnet
};

enum class EraseOption {
    NONE,           // No erase
    ERASE,          // Erase screen
    ERASEAUP        // Erase all unprotected
};

enum class CursorOption {
    NONE,           // No cursor positioning
    SET,            // Set cursor position
    HOME            // Home cursor
};

enum class WaitOption {
    WAIT,           // Wait for response
    NOWAIT          // Don't wait
};

// AID Keys (Attention Identifier)
enum class AIDKey : UInt8 {
    NONE = 0x00,
    ENTER = 0x7D,
    CLEAR = 0x6D,
    PA1 = 0x6C,
    PA2 = 0x6E,
    PA3 = 0x6B,
    PF1 = 0xF1,
    PF2 = 0xF2,
    PF3 = 0xF3,
    PF4 = 0xF4,
    PF5 = 0xF5,
    PF6 = 0xF6,
    PF7 = 0xF7,
    PF8 = 0xF8,
    PF9 = 0xF9,
    PF10 = 0x7A,
    PF11 = 0x7B,
    PF12 = 0x7C,
    PF13 = 0xC1,
    PF14 = 0xC2,
    PF15 = 0xC3,
    PF16 = 0xC4,
    PF17 = 0xC5,
    PF18 = 0xC6,
    PF19 = 0xC7,
    PF20 = 0xC8,
    PF21 = 0xC9,
    PF22 = 0x4A,
    PF23 = 0x4B,
    PF24 = 0x4C
};

// =============================================================================
// Send Options
// =============================================================================

struct SendOptions {
    EraseOption erase = EraseOption::NONE;
    CursorOption cursor = CursorOption::NONE;
    UInt16 cursor_row = 1;
    UInt16 cursor_col = 1;
    bool freekb = false;        // Free keyboard
    bool alarm = false;         // Sound alarm
    bool last = false;          // Last in chain
    bool accum = false;         // Accumulate output
    bool paging = false;        // Paging mode
    bool wait = true;           // Wait for completion
    UInt16 length = 0;          // Data length (0 = calculate)
};

// =============================================================================
// Receive Options
// =============================================================================

struct ReceiveOptions {
    UInt32 max_length = MAX_INPUT_LENGTH;
    bool into_buffer = true;    // Receive into buffer
    bool asis = false;          // As-is (no translation)
    UInt32 timeout_ms = 0;      // 0 = no timeout
};

// =============================================================================
// Terminal Input
// =============================================================================

struct TerminalInput {
    AIDKey aid_key = AIDKey::NONE;
    UInt16 cursor_row = 1;
    UInt16 cursor_col = 1;
    ByteBuffer data;
    std::chrono::steady_clock::time_point received;
};

// =============================================================================
// Terminal State
// =============================================================================

struct TerminalState {
    String terminal_id;
    TerminalType type;
    UInt16 rows;
    UInt16 cols;
    UInt16 cursor_row;
    UInt16 cursor_col;
    bool connected;
    bool keyboard_locked;
    AIDKey last_aid;
};

// =============================================================================
// Terminal Session
// =============================================================================

class TerminalSession {
public:
    TerminalSession(StringView terminal_id, TerminalType type = TerminalType::IBM_3270);
    ~TerminalSession();
    
    // Properties
    [[nodiscard]] const String& terminal_id() const { return terminal_id_; }
    [[nodiscard]] TerminalType type() const { return type_; }
    [[nodiscard]] UInt16 rows() const { return rows_; }
    [[nodiscard]] UInt16 cols() const { return cols_; }
    [[nodiscard]] bool is_connected() const { return connected_; }
    
    // Screen dimensions
    void set_dimensions(UInt16 rows, UInt16 cols);
    
    // Connection
    void connect();
    void disconnect();
    
    // Send operations
    Result<void> send_text(StringView text, const SendOptions& opts = {});
    Result<void> send_data(const ByteBuffer& data, const SendOptions& opts = {});
    Result<void> send_control(const SendOptions& opts);
    
    // Receive operations
    Result<TerminalInput> receive(const ReceiveOptions& opts = {});
    Result<String> receive_text(UInt32 max_length = MAX_INPUT_LENGTH);
    
    // Cursor control
    void set_cursor(UInt16 row, UInt16 col);
    [[nodiscard]] std::pair<UInt16, UInt16> get_cursor() const;
    
    // Screen buffer access
    [[nodiscard]] const ByteBuffer& screen_buffer() const { return screen_buffer_; }
    [[nodiscard]] String get_screen_text() const;
    [[nodiscard]] String get_screen_line(UInt16 row) const;
    void clear_screen();
    
    // Input simulation (for testing)
    void simulate_input(const TerminalInput& input);
    void simulate_key(AIDKey key, StringView text = "");
    
    // State
    [[nodiscard]] TerminalState get_state() const;
    
private:
    void write_to_screen(UInt16 row, UInt16 col, StringView text);
    
    String terminal_id_;
    TerminalType type_;
    UInt16 rows_ = DEFAULT_SCREEN_ROWS;
    UInt16 cols_ = DEFAULT_SCREEN_COLS;
    UInt16 cursor_row_ = 1;
    UInt16 cursor_col_ = 1;
    bool connected_ = false;
    bool keyboard_locked_ = false;
    AIDKey last_aid_ = AIDKey::NONE;
    
    ByteBuffer screen_buffer_;
    std::queue<TerminalInput> input_queue_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Terminal Statistics
// =============================================================================

struct TerminalStats {
    UInt64 sends_executed{0};
    UInt64 receives_executed{0};
    UInt64 bytes_sent{0};
    UInt64 bytes_received{0};
    UInt64 sessions_created{0};
    UInt64 sessions_closed{0};
    UInt64 timeouts{0};
};

// =============================================================================
// Terminal Manager
// =============================================================================

class TerminalManager {
public:
    static TerminalManager& instance();
    
    // Lifecycle
    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }
    
    // Session management
    Result<TerminalSession*> create_session(StringView terminal_id,
                                             TerminalType type = TerminalType::IBM_3270);
    Result<TerminalSession*> get_session(StringView terminal_id);
    Result<void> close_session(StringView terminal_id);
    [[nodiscard]] bool has_session(StringView terminal_id) const;
    
    // Current terminal
    void set_current_terminal(StringView terminal_id);
    [[nodiscard]] TerminalSession* current_terminal();
    [[nodiscard]] const String& current_terminal_id() const { return current_terminal_id_; }
    
    // Operations on current terminal
    Result<void> send(StringView text, const SendOptions& opts = {});
    Result<void> send(const ByteBuffer& data, const SendOptions& opts = {});
    Result<TerminalInput> receive(const ReceiveOptions& opts = {});
    Result<void> control(const SendOptions& opts);
    
    // List operations
    [[nodiscard]] std::vector<String> list_terminals() const;
    [[nodiscard]] std::vector<TerminalState> list_terminal_states() const;
    
    // Statistics
    [[nodiscard]] TerminalStats get_stats() const;
    void reset_stats();
    
    // Output callback (for integration with actual terminals)
    using OutputCallback = std::function<void(StringView terminal_id, const ByteBuffer& data)>;
    void set_output_callback(OutputCallback callback);
    
private:
    TerminalManager() = default;
    ~TerminalManager() = default;
    TerminalManager(const TerminalManager&) = delete;
    TerminalManager& operator=(const TerminalManager&) = delete;
    
    bool initialized_ = false;
    std::unordered_map<String, std::unique_ptr<TerminalSession>> sessions_;
    thread_local static String current_terminal_id_;
    TerminalStats stats_;
    OutputCallback output_callback_;
    mutable std::mutex mutex_;
};

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

// SEND TEXT
Result<void> exec_cics_send_text(StringView text);
Result<void> exec_cics_send_text(StringView text, const SendOptions& opts);
Result<void> exec_cics_send_text_erase(StringView text);
Result<void> exec_cics_send_text_freekb(StringView text);
Result<void> exec_cics_send_text_alarm(StringView text);

// SEND DATA
Result<void> exec_cics_send(const ByteBuffer& data);
Result<void> exec_cics_send(const ByteBuffer& data, const SendOptions& opts);

// SEND CONTROL
Result<void> exec_cics_send_control_erase();
Result<void> exec_cics_send_control_freekb();
Result<void> exec_cics_send_control_alarm();
Result<void> exec_cics_send_control_cursor(UInt16 row, UInt16 col);

// RECEIVE
Result<TerminalInput> exec_cics_receive();
Result<TerminalInput> exec_cics_receive(const ReceiveOptions& opts);
Result<String> exec_cics_receive_text();
Result<UInt32> exec_cics_receive_into(void* buffer, UInt32 max_length);

// CONVERSE (SEND + RECEIVE)
Result<TerminalInput> exec_cics_converse(StringView text);
Result<TerminalInput> exec_cics_converse(const ByteBuffer& data);

// =============================================================================
// Utility Functions
// =============================================================================

[[nodiscard]] String aid_key_to_string(AIDKey key);
[[nodiscard]] AIDKey string_to_aid_key(StringView str);
[[nodiscard]] String terminal_type_to_string(TerminalType type);

} // namespace terminal
} // namespace cics

#endif // CICS_TERMINAL_HPP
