// =============================================================================
// CICS Emulation - Basic Mapping Support (BMS) Module
// Version: 3.4.6
// =============================================================================
// Implements CICS BMS services for screen/map handling:
// - Map definitions and field attributes
// - SEND MAP / RECEIVE MAP operations
// - Screen buffer management
// - Field validation
// =============================================================================

#ifndef CICS_BMS_HPP
#define CICS_BMS_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace cics {
namespace bms {

// =============================================================================
// Screen Dimensions
// =============================================================================

constexpr UInt16 DEFAULT_SCREEN_ROWS = 24;
constexpr UInt16 DEFAULT_SCREEN_COLS = 80;
constexpr UInt16 MAX_SCREEN_ROWS = 43;
constexpr UInt16 MAX_SCREEN_COLS = 132;

// =============================================================================
// Field Attributes
// =============================================================================

enum class FieldAttribute : Byte {
    UNPROT_NORM = 0x00,      // Unprotected, normal intensity
    UNPROT_NORM_MDT = 0x01,  // Unprotected, normal, MDT on
    UNPROT_BRT = 0x08,       // Unprotected, bright
    UNPROT_BRT_MDT = 0x09,   // Unprotected, bright, MDT on
    UNPROT_DARK = 0x0C,      // Unprotected, dark (hidden)
    UNPROT_DARK_MDT = 0x0D,  // Unprotected, dark, MDT on
    PROT_NORM = 0x20,        // Protected, normal
    PROT_NORM_MDT = 0x21,    // Protected, normal, MDT on
    PROT_BRT = 0x28,         // Protected, bright
    PROT_BRT_MDT = 0x29,     // Protected, bright, MDT on
    PROT_DARK = 0x2C,        // Protected, dark
    PROT_DARK_MDT = 0x2D,    // Protected, dark, MDT on
    ASKIP_NORM = 0x30,       // Autoskip, normal
    ASKIP_BRT = 0x38,        // Autoskip, bright
    ASKIP_DARK = 0x3C,       // Autoskip, dark
    NUMERIC = 0x10,          // Numeric only modifier
    CURSOR = 0x40            // Initial cursor position
};

// Extended attributes
enum class ExtendedAttribute : Byte {
    DEFAULT = 0x00,
    UNDERSCORE = 0x01,
    BLINK = 0x02,
    REVERSE = 0x04,
    BLUE = 0x10,
    RED = 0x20,
    PINK = 0x30,
    GREEN = 0x40,
    TURQUOISE = 0x50,
    YELLOW = 0x60,
    WHITE = 0x70
};

// Justification
enum class Justify : UInt8 {
    LEFT,
    RIGHT,
    ZERO_FILL
};

// =============================================================================
// Field Definition
// =============================================================================

struct FieldDefinition {
    String name;
    UInt16 row = 1;
    UInt16 col = 1;
    UInt16 length = 1;
    FieldAttribute attribute = FieldAttribute::UNPROT_NORM;
    ExtendedAttribute extended = ExtendedAttribute::DEFAULT;
    Justify justify = Justify::LEFT;
    String initial_value;
    String picture;           // For validation (e.g., "9(5)" for numeric)
    bool occurs = false;      // Array field
    UInt16 occurs_count = 0;  // Number of occurrences
    
    [[nodiscard]] bool is_protected() const;
    [[nodiscard]] bool is_numeric() const;
    [[nodiscard]] bool is_bright() const;
    [[nodiscard]] bool is_dark() const;
    [[nodiscard]] bool has_mdt() const;
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// Map Definition
// =============================================================================

struct MapDefinition {
    FixedString<8> map_name;
    FixedString<8> mapset_name;
    UInt16 rows = DEFAULT_SCREEN_ROWS;
    UInt16 cols = DEFAULT_SCREEN_COLS;
    std::vector<FieldDefinition> fields;
    String title;
    bool cursor_home = true;  // Position cursor at first unprotected field
    
    [[nodiscard]] const FieldDefinition* find_field(StringView name) const;
    [[nodiscard]] FieldDefinition* find_field(StringView name);
    [[nodiscard]] std::vector<const FieldDefinition*> get_unprotected_fields() const;
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// Screen Buffer
// =============================================================================

class ScreenBuffer {
private:
    UInt16 rows_;
    UInt16 cols_;
    std::vector<Byte> data_;
    std::vector<Byte> attributes_;
    UInt16 cursor_row_ = 1;
    UInt16 cursor_col_ = 1;
    
public:
    ScreenBuffer(UInt16 rows = DEFAULT_SCREEN_ROWS, UInt16 cols = DEFAULT_SCREEN_COLS);
    
    // Position operations
    void set_cursor(UInt16 row, UInt16 col);
    [[nodiscard]] UInt16 cursor_row() const { return cursor_row_; }
    [[nodiscard]] UInt16 cursor_col() const { return cursor_col_; }
    
    // Data operations
    void clear();
    void write(UInt16 row, UInt16 col, StringView text);
    void write(UInt16 row, UInt16 col, const Byte* data, UInt16 length);
    void write_attribute(UInt16 row, UInt16 col, FieldAttribute attr);
    [[nodiscard]] String read(UInt16 row, UInt16 col, UInt16 length) const;
    [[nodiscard]] Byte get_char(UInt16 row, UInt16 col) const;
    [[nodiscard]] FieldAttribute get_attribute(UInt16 row, UInt16 col) const;
    
    // Field operations
    void write_field(const FieldDefinition& field, StringView value);
    [[nodiscard]] String read_field(const FieldDefinition& field) const;
    
    // Buffer access
    [[nodiscard]] const Byte* data() const { return data_.data(); }
    [[nodiscard]] UInt32 size() const { return static_cast<UInt32>(data_.size()); }
    [[nodiscard]] UInt16 rows() const { return rows_; }
    [[nodiscard]] UInt16 cols() const { return cols_; }
    
    // Render to string
    [[nodiscard]] String render() const;
    [[nodiscard]] String render_with_attrs() const;
    
private:
    [[nodiscard]] UInt32 offset(UInt16 row, UInt16 col) const;
};

// =============================================================================
// Map Data Structure (for SEND/RECEIVE MAP)
// =============================================================================

struct MapFieldData {
    String name;
    ByteBuffer data;
    UInt16 length = 0;
    bool modified = false;
    FieldAttribute attribute = FieldAttribute::UNPROT_NORM;
};

struct MapData {
    FixedString<8> map_name;
    FixedString<8> mapset_name;
    std::unordered_map<String, MapFieldData> fields;
    UInt16 cursor_row = 0;
    UInt16 cursor_col = 0;
    
    void set_field(StringView name, StringView value);
    void set_field(StringView name, const ByteBuffer& value);
    [[nodiscard]] String get_field(StringView name) const;
    [[nodiscard]] const ByteBuffer* get_field_data(StringView name) const;
    [[nodiscard]] bool is_field_modified(StringView name) const;
    void clear();
};

// =============================================================================
// BMS Manager
// =============================================================================

// Terminal output callback
using TerminalOutputCallback = std::function<void(const ScreenBuffer&)>;

class BMSManager {
private:
    mutable std::mutex mutex_;
    
    // Map definitions (mapset -> map_name -> definition)
    std::unordered_map<String, std::unordered_map<String, MapDefinition>> mapsets_;
    
    // Current screen state per terminal
    std::unordered_map<String, ScreenBuffer> terminal_buffers_;
    
    // Terminal callback
    TerminalOutputCallback output_callback_;
    
    // Statistics
    struct Statistics {
        UInt64 send_map_count = 0;
        UInt64 receive_map_count = 0;
        UInt64 send_text_count = 0;
        UInt64 send_control_count = 0;
    } stats_;
    
public:
    BMSManager() = default;
    ~BMSManager() = default;
    
    // Singleton access
    static BMSManager& instance();
    
    // Map definition management
    Result<void> define_mapset(StringView mapset_name);
    Result<void> define_map(const MapDefinition& map);
    Result<void> define_map(StringView mapset_name, const MapDefinition& map);
    Result<const MapDefinition*> get_map(StringView mapset_name, StringView map_name) const;
    [[nodiscard]] std::vector<String> list_mapsets() const;
    [[nodiscard]] std::vector<String> list_maps(StringView mapset_name) const;
    
    // SEND MAP - Display map on terminal
    Result<void> send_map(StringView map_name, StringView mapset_name);
    Result<void> send_map(StringView map_name, StringView mapset_name, const MapData& data);
    Result<void> send_map(StringView map_name, StringView mapset_name, const MapData& data,
                          StringView terminal_id);
    
    // RECEIVE MAP - Get data from terminal
    Result<MapData> receive_map(StringView map_name, StringView mapset_name);
    Result<MapData> receive_map(StringView map_name, StringView mapset_name,
                                StringView terminal_id);
    
    // SEND TEXT - Send unformatted text
    Result<void> send_text(StringView text);
    Result<void> send_text(StringView text, StringView terminal_id);
    
    // SEND CONTROL - Send control commands
    Result<void> send_control_erase();
    Result<void> send_control_alarm();
    Result<void> send_control_cursor(UInt16 row, UInt16 col);
    Result<void> send_control_freekb();
    
    // Terminal buffer management
    Result<ScreenBuffer*> get_terminal_buffer(StringView terminal_id);
    Result<void> create_terminal(StringView terminal_id, UInt16 rows = DEFAULT_SCREEN_ROWS,
                                  UInt16 cols = DEFAULT_SCREEN_COLS);
    Result<void> destroy_terminal(StringView terminal_id);
    
    // Simulate terminal input
    Result<void> simulate_input(StringView terminal_id, const MapData& input);
    Result<void> simulate_key(StringView terminal_id, UInt8 aid_key);
    
    // Set output callback
    void set_output_callback(TerminalOutputCallback callback);
    
    // Statistics
    [[nodiscard]] String get_statistics() const;
    void reset_statistics();
};

// =============================================================================
// AID Keys
// =============================================================================

namespace aid {
    constexpr Byte ENTER = 0x7D;
    constexpr Byte CLEAR = 0x6D;
    constexpr Byte PA1 = 0x6C;
    constexpr Byte PA2 = 0x6E;
    constexpr Byte PA3 = 0x6B;
    constexpr Byte PF1 = 0xF1;
    constexpr Byte PF2 = 0xF2;
    constexpr Byte PF3 = 0xF3;
    constexpr Byte PF4 = 0xF4;
    constexpr Byte PF5 = 0xF5;
    constexpr Byte PF6 = 0xF6;
    constexpr Byte PF7 = 0xF7;
    constexpr Byte PF8 = 0xF8;
    constexpr Byte PF9 = 0xF9;
    constexpr Byte PF10 = 0x7A;
    constexpr Byte PF11 = 0x7B;
    constexpr Byte PF12 = 0x7C;
    constexpr Byte PF13 = 0xC1;
    constexpr Byte PF14 = 0xC2;
    constexpr Byte PF15 = 0xC3;
    constexpr Byte PF16 = 0xC4;
    constexpr Byte PF17 = 0xC5;
    constexpr Byte PF18 = 0xC6;
    constexpr Byte PF19 = 0xC7;
    constexpr Byte PF20 = 0xC8;
    constexpr Byte PF21 = 0xC9;
    constexpr Byte PF22 = 0x4A;
    constexpr Byte PF23 = 0x4B;
    constexpr Byte PF24 = 0x4C;
    
    [[nodiscard]] String aid_to_string(Byte aid);
}

// =============================================================================
// EXEC CICS Style Interface
// =============================================================================

Result<void> exec_cics_send_map(StringView map, StringView mapset);
Result<void> exec_cics_send_map(StringView map, StringView mapset, const MapData& data);
Result<MapData> exec_cics_receive_map(StringView map, StringView mapset);
Result<void> exec_cics_send_text(StringView text);
Result<void> exec_cics_send_control_erase();

// =============================================================================
// Map Definition Builder
// =============================================================================

class MapBuilder {
private:
    MapDefinition map_;
    
public:
    MapBuilder(StringView map_name, StringView mapset_name);
    
    MapBuilder& size(UInt16 rows, UInt16 cols);
    MapBuilder& title(StringView title);
    
    MapBuilder& field(StringView name, UInt16 row, UInt16 col, UInt16 length);
    MapBuilder& field(StringView name, UInt16 row, UInt16 col, UInt16 length,
                     FieldAttribute attr);
    MapBuilder& field(StringView name, UInt16 row, UInt16 col, UInt16 length,
                     FieldAttribute attr, StringView initial);
    
    MapBuilder& label(UInt16 row, UInt16 col, StringView text);
    MapBuilder& label(UInt16 row, UInt16 col, StringView text, FieldAttribute attr);
    
    [[nodiscard]] MapDefinition build() const { return map_; }
    Result<void> register_map();
};

} // namespace bms
} // namespace cics

#endif // CICS_BMS_HPP
