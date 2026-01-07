// =============================================================================
// CICS Emulation - Basic Mapping Support (BMS) Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/bms/bms.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace cics {
namespace bms {

// =============================================================================
// FieldDefinition Implementation
// =============================================================================

bool FieldDefinition::is_protected() const {
    Byte attr = static_cast<Byte>(attribute);
    return (attr & 0x20) != 0;
}

bool FieldDefinition::is_numeric() const {
    Byte attr = static_cast<Byte>(attribute);
    return (attr & 0x10) != 0;
}

bool FieldDefinition::is_bright() const {
    Byte attr = static_cast<Byte>(attribute);
    return (attr & 0x08) != 0;
}

bool FieldDefinition::is_dark() const {
    Byte attr = static_cast<Byte>(attribute);
    return (attr & 0x0C) == 0x0C;
}

bool FieldDefinition::has_mdt() const {
    Byte attr = static_cast<Byte>(attribute);
    return (attr & 0x01) != 0;
}

String FieldDefinition::to_string() const {
    std::ostringstream oss;
    oss << "Field{" << name << " @" << row << "," << col << " len=" << length << "}";
    return oss.str();
}

// =============================================================================
// MapDefinition Implementation
// =============================================================================

const FieldDefinition* MapDefinition::find_field(StringView name) const {
    for (const auto& field : fields) {
        if (field.name == name) {
            return &field;
        }
    }
    return nullptr;
}

FieldDefinition* MapDefinition::find_field(StringView name) {
    for (auto& field : fields) {
        if (field.name == name) {
            return &field;
        }
    }
    return nullptr;
}

std::vector<const FieldDefinition*> MapDefinition::get_unprotected_fields() const {
    std::vector<const FieldDefinition*> result;
    for (const auto& field : fields) {
        if (!field.is_protected()) {
            result.push_back(&field);
        }
    }
    return result;
}

String MapDefinition::to_string() const {
    std::ostringstream oss;
    oss << "Map{" << String(map_name.data(), 8) << " in " 
        << String(mapset_name.data(), 8) << ", " << rows << "x" << cols
        << ", fields=" << fields.size() << "}";
    return oss.str();
}

// =============================================================================
// ScreenBuffer Implementation
// =============================================================================

ScreenBuffer::ScreenBuffer(UInt16 rows, UInt16 cols)
    : rows_(rows), cols_(cols),
      data_(rows * cols, ' '),
      attributes_(rows * cols, static_cast<Byte>(FieldAttribute::UNPROT_NORM)) {
}

void ScreenBuffer::set_cursor(UInt16 row, UInt16 col) {
    cursor_row_ = std::min(row, rows_);
    cursor_col_ = std::min(col, cols_);
}

void ScreenBuffer::clear() {
    std::fill(data_.begin(), data_.end(), ' ');
    std::fill(attributes_.begin(), attributes_.end(), 
              static_cast<Byte>(FieldAttribute::UNPROT_NORM));
    cursor_row_ = 1;
    cursor_col_ = 1;
}

void ScreenBuffer::write(UInt16 row, UInt16 col, StringView text) {
    write(row, col, reinterpret_cast<const Byte*>(text.data()), 
          static_cast<UInt16>(text.length()));
}

void ScreenBuffer::write(UInt16 row, UInt16 col, const Byte* data, UInt16 length) {
    if (row < 1 || row > rows_ || col < 1 || col > cols_) return;
    
    UInt32 start = offset(row, col);
    UInt16 max_len = std::min(length, static_cast<UInt16>(cols_ - col + 1));
    
    for (UInt16 i = 0; i < max_len && start + i < data_.size(); ++i) {
        data_[start + i] = data[i];
    }
}

void ScreenBuffer::write_attribute(UInt16 row, UInt16 col, FieldAttribute attr) {
    if (row < 1 || row > rows_ || col < 1 || col > cols_) return;
    attributes_[offset(row, col)] = static_cast<Byte>(attr);
}

String ScreenBuffer::read(UInt16 row, UInt16 col, UInt16 length) const {
    if (row < 1 || row > rows_ || col < 1 || col > cols_) return "";
    
    UInt32 start = offset(row, col);
    UInt16 max_len = std::min(length, static_cast<UInt16>(cols_ - col + 1));
    
    String result;
    for (UInt16 i = 0; i < max_len && start + i < data_.size(); ++i) {
        result += static_cast<char>(data_[start + i]);
    }
    return result;
}

Byte ScreenBuffer::get_char(UInt16 row, UInt16 col) const {
    if (row < 1 || row > rows_ || col < 1 || col > cols_) return ' ';
    return data_[offset(row, col)];
}

FieldAttribute ScreenBuffer::get_attribute(UInt16 row, UInt16 col) const {
    if (row < 1 || row > rows_ || col < 1 || col > cols_) 
        return FieldAttribute::UNPROT_NORM;
    return static_cast<FieldAttribute>(attributes_[offset(row, col)]);
}

void ScreenBuffer::write_field(const FieldDefinition& field, StringView value) {
    write(field.row, field.col, value);
    write_attribute(field.row, field.col, field.attribute);
}

String ScreenBuffer::read_field(const FieldDefinition& field) const {
    return read(field.row, field.col, field.length);
}

UInt32 ScreenBuffer::offset(UInt16 row, UInt16 col) const {
    return static_cast<UInt32>((row - 1) * cols_ + (col - 1));
}

String ScreenBuffer::render() const {
    std::ostringstream oss;
    
    // Top border
    oss << "+" << String(cols_, '-') << "+\n";
    
    for (UInt16 r = 1; r <= rows_; ++r) {
        oss << "|";
        for (UInt16 c = 1; c <= cols_; ++c) {
            char ch = static_cast<char>(get_char(r, c));
            oss << (ch >= 32 && ch < 127 ? ch : ' ');
        }
        oss << "|\n";
    }
    
    // Bottom border
    oss << "+" << String(cols_, '-') << "+\n";
    oss << "Cursor: " << cursor_row_ << "," << cursor_col_ << "\n";
    
    return oss.str();
}

String ScreenBuffer::render_with_attrs() const {
    return render();  // Simplified - could add color codes
}

// =============================================================================
// MapData Implementation
// =============================================================================

void MapData::set_field(StringView name, StringView value) {
    MapFieldData& field = fields[String(name)];
    field.name = String(name);
    field.data.assign(value.begin(), value.end());
    field.length = static_cast<UInt16>(value.length());
    field.modified = true;
}

void MapData::set_field(StringView name, const ByteBuffer& value) {
    MapFieldData& field = fields[String(name)];
    field.name = String(name);
    field.data = value;
    field.length = static_cast<UInt16>(value.size());
    field.modified = true;
}

String MapData::get_field(StringView name) const {
    auto it = fields.find(String(name));
    if (it == fields.end()) return "";
    return String(it->second.data.begin(), it->second.data.end());
}

const ByteBuffer* MapData::get_field_data(StringView name) const {
    auto it = fields.find(String(name));
    if (it == fields.end()) return nullptr;
    return &it->second.data;
}

bool MapData::is_field_modified(StringView name) const {
    auto it = fields.find(String(name));
    if (it == fields.end()) return false;
    return it->second.modified;
}

void MapData::clear() {
    fields.clear();
    cursor_row = 0;
    cursor_col = 0;
}

// =============================================================================
// BMSManager Implementation
// =============================================================================

BMSManager& BMSManager::instance() {
    static BMSManager instance;
    return instance;
}

Result<void> BMSManager::define_mapset(StringView mapset_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    mapsets_[String(mapset_name)] = {};
    return make_success();
}

Result<void> BMSManager::define_map(const MapDefinition& map) {
    String mapset(map.mapset_name.data(), 8);
    mapset.erase(std::find_if(mapset.rbegin(), mapset.rend(),
        [](unsigned char ch) { return !std::isspace(ch); }).base(), mapset.end());
    return define_map(mapset, map);
}

Result<void> BMSManager::define_map(StringView mapset_name, const MapDefinition& map) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    String mapset(mapset_name);
    String map_name(map.map_name.data(), 8);
    map_name.erase(std::find_if(map_name.rbegin(), map_name.rend(),
        [](unsigned char ch) { return !std::isspace(ch); }).base(), map_name.end());
    
    mapsets_[mapset][map_name] = map;
    return make_success();
}

Result<const MapDefinition*> BMSManager::get_map(StringView mapset_name, 
                                                  StringView map_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto mapset_it = mapsets_.find(String(mapset_name));
    if (mapset_it == mapsets_.end()) {
        return make_error<const MapDefinition*>(ErrorCode::RECORD_NOT_FOUND,
            "Mapset not found: " + String(mapset_name));
    }
    
    auto map_it = mapset_it->second.find(String(map_name));
    if (map_it == mapset_it->second.end()) {
        return make_error<const MapDefinition*>(ErrorCode::RECORD_NOT_FOUND,
            "Map not found: " + String(map_name));
    }
    
    return make_success(&map_it->second);
}

std::vector<String> BMSManager::list_mapsets() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<String> result;
    for (const auto& [name, maps] : mapsets_) {
        result.push_back(name);
    }
    return result;
}

std::vector<String> BMSManager::list_maps(StringView mapset_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<String> result;
    
    auto it = mapsets_.find(String(mapset_name));
    if (it != mapsets_.end()) {
        for (const auto& [name, map] : it->second) {
            result.push_back(name);
        }
    }
    return result;
}

Result<void> BMSManager::send_map(StringView map_name, StringView mapset_name) {
    MapData empty_data;
    return send_map(map_name, mapset_name, empty_data);
}

Result<void> BMSManager::send_map(StringView map_name, StringView mapset_name,
                                   const MapData& data) {
    return send_map(map_name, mapset_name, data, "DEFAULT");
}

Result<void> BMSManager::send_map(StringView map_name, StringView mapset_name,
                                   const MapData& data, StringView terminal_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.send_map_count;
    
    // Get map definition
    auto mapset_it = mapsets_.find(String(mapset_name));
    if (mapset_it == mapsets_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Mapset not found: " + String(mapset_name));
    }
    
    auto map_it = mapset_it->second.find(String(map_name));
    if (map_it == mapset_it->second.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Map not found: " + String(map_name));
    }
    
    const MapDefinition& map_def = map_it->second;
    
    // Get or create terminal buffer
    auto& buffer = terminal_buffers_[String(terminal_id)];
    if (buffer.rows() == 0) {
        buffer = ScreenBuffer(map_def.rows, map_def.cols);
    }
    
    // Clear and populate buffer
    buffer.clear();
    
    // Write field labels and initial values
    for (const auto& field : map_def.fields) {
        String value = field.initial_value;
        
        // Check for data override
        auto field_it = data.fields.find(field.name);
        if (field_it != data.fields.end()) {
            value = String(field_it->second.data.begin(), field_it->second.data.end());
        }
        
        // Pad or truncate to field length
        if (value.length() < field.length) {
            value.append(field.length - value.length(), ' ');
        } else if (value.length() > field.length) {
            value = value.substr(0, field.length);
        }
        
        buffer.write_field(field, value);
    }
    
    // Set cursor position
    if (data.cursor_row > 0 && data.cursor_col > 0) {
        buffer.set_cursor(data.cursor_row, data.cursor_col);
    } else if (map_def.cursor_home) {
        auto unprotected = map_def.get_unprotected_fields();
        if (!unprotected.empty()) {
            buffer.set_cursor(unprotected[0]->row, unprotected[0]->col);
        }
    }
    
    // Call output callback if set
    if (output_callback_) {
        output_callback_(buffer);
    }
    
    return make_success();
}

Result<MapData> BMSManager::receive_map(StringView map_name, StringView mapset_name) {
    return receive_map(map_name, mapset_name, "DEFAULT");
}

Result<MapData> BMSManager::receive_map(StringView map_name, StringView mapset_name,
                                         StringView terminal_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.receive_map_count;
    
    // Get map definition
    auto mapset_it = mapsets_.find(String(mapset_name));
    if (mapset_it == mapsets_.end()) {
        return make_error<MapData>(ErrorCode::RECORD_NOT_FOUND,
            "Mapset not found: " + String(mapset_name));
    }
    
    auto map_it = mapset_it->second.find(String(map_name));
    if (map_it == mapset_it->second.end()) {
        return make_error<MapData>(ErrorCode::RECORD_NOT_FOUND,
            "Map not found: " + String(map_name));
    }
    
    const MapDefinition& map_def = map_it->second;
    
    // Get terminal buffer
    auto buffer_it = terminal_buffers_.find(String(terminal_id));
    if (buffer_it == terminal_buffers_.end()) {
        return make_error<MapData>(ErrorCode::RECORD_NOT_FOUND,
            "Terminal not found: " + String(terminal_id));
    }
    
    const ScreenBuffer& buffer = buffer_it->second;
    
    // Read field values from buffer
    MapData result;
    result.map_name = map_def.map_name;
    result.mapset_name = map_def.mapset_name;
    result.cursor_row = buffer.cursor_row();
    result.cursor_col = buffer.cursor_col();
    
    for (const auto& field : map_def.fields) {
        if (!field.is_protected()) {
            String value = buffer.read_field(field);
            result.set_field(field.name, value);
        }
    }
    
    return make_success(std::move(result));
}

Result<void> BMSManager::send_text(StringView text) {
    return send_text(text, "DEFAULT");
}

Result<void> BMSManager::send_text(StringView text, StringView terminal_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.send_text_count;
    
    auto& buffer = terminal_buffers_[String(terminal_id)];
    if (buffer.rows() == 0) {
        buffer = ScreenBuffer();
    }
    
    buffer.write(buffer.cursor_row(), buffer.cursor_col(), text);
    
    if (output_callback_) {
        output_callback_(buffer);
    }
    
    return make_success();
}

Result<void> BMSManager::send_control_erase() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.send_control_count;
    
    for (auto& [id, buffer] : terminal_buffers_) {
        buffer.clear();
    }
    
    return make_success();
}

Result<void> BMSManager::send_control_alarm() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.send_control_count;
    // Would trigger terminal bell
    return make_success();
}

Result<void> BMSManager::send_control_cursor(UInt16 row, UInt16 col) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.send_control_count;
    
    for (auto& [id, buffer] : terminal_buffers_) {
        buffer.set_cursor(row, col);
    }
    
    return make_success();
}

Result<void> BMSManager::send_control_freekb() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.send_control_count;
    // Would unlock keyboard
    return make_success();
}

Result<ScreenBuffer*> BMSManager::get_terminal_buffer(StringView terminal_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = terminal_buffers_.find(String(terminal_id));
    if (it == terminal_buffers_.end()) {
        return make_error<ScreenBuffer*>(ErrorCode::RECORD_NOT_FOUND,
            "Terminal not found: " + String(terminal_id));
    }
    
    return make_success(&it->second);
}

Result<void> BMSManager::create_terminal(StringView terminal_id, UInt16 rows, UInt16 cols) {
    std::lock_guard<std::mutex> lock(mutex_);
    terminal_buffers_[String(terminal_id)] = ScreenBuffer(rows, cols);
    return make_success();
}

Result<void> BMSManager::destroy_terminal(StringView terminal_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    terminal_buffers_.erase(String(terminal_id));
    return make_success();
}

Result<void> BMSManager::simulate_input(StringView terminal_id, const MapData& input) {
    (void)input;  // Reserved for future terminal simulation
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = terminal_buffers_.find(String(terminal_id));
    if (it == terminal_buffers_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Terminal not found: " + String(terminal_id));
    }
    
    // Would write input data to buffer
    return make_success();
}

Result<void> BMSManager::simulate_key(StringView terminal_id, UInt8 aid_key) {
    (void)terminal_id;  // Reserved for future terminal simulation
    (void)aid_key;      // Reserved for future terminal simulation
    
    std::lock_guard<std::mutex> lock(mutex_);
    // Would process AID key
    return make_success();
}

void BMSManager::set_output_callback(TerminalOutputCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    output_callback_ = std::move(callback);
}

String BMSManager::get_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "BMS Statistics:\n"
        << "  SEND MAP:     " << stats_.send_map_count << "\n"
        << "  RECEIVE MAP:  " << stats_.receive_map_count << "\n"
        << "  SEND TEXT:    " << stats_.send_text_count << "\n"
        << "  SEND CONTROL: " << stats_.send_control_count << "\n"
        << "  Mapsets:      " << mapsets_.size() << "\n"
        << "  Terminals:    " << terminal_buffers_.size() << "\n";
    
    return oss.str();
}

void BMSManager::reset_statistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Statistics{};
}

// =============================================================================
// AID Key Functions
// =============================================================================

namespace aid {
    String aid_to_string(Byte aid_key) {
        switch (aid_key) {
            case ENTER: return "ENTER";
            case CLEAR: return "CLEAR";
            case PA1: return "PA1";
            case PA2: return "PA2";
            case PA3: return "PA3";
            case PF1: return "PF1";
            case PF2: return "PF2";
            case PF3: return "PF3";
            case PF4: return "PF4";
            case PF5: return "PF5";
            case PF6: return "PF6";
            case PF7: return "PF7";
            case PF8: return "PF8";
            case PF9: return "PF9";
            case PF10: return "PF10";
            case PF11: return "PF11";
            case PF12: return "PF12";
            default: return "UNKNOWN";
        }
    }
}

// =============================================================================
// EXEC CICS Interface
// =============================================================================

Result<void> exec_cics_send_map(StringView map, StringView mapset) {
    return BMSManager::instance().send_map(map, mapset);
}

Result<void> exec_cics_send_map(StringView map, StringView mapset, const MapData& data) {
    return BMSManager::instance().send_map(map, mapset, data);
}

Result<MapData> exec_cics_receive_map(StringView map, StringView mapset) {
    return BMSManager::instance().receive_map(map, mapset);
}

Result<void> exec_cics_send_text(StringView text) {
    return BMSManager::instance().send_text(text);
}

Result<void> exec_cics_send_control_erase() {
    return BMSManager::instance().send_control_erase();
}

// =============================================================================
// MapBuilder Implementation
// =============================================================================

MapBuilder::MapBuilder(StringView map_name, StringView mapset_name) {
    map_.map_name = map_name;
    map_.mapset_name = mapset_name;
}

MapBuilder& MapBuilder::size(UInt16 rows, UInt16 cols) {
    map_.rows = rows;
    map_.cols = cols;
    return *this;
}

MapBuilder& MapBuilder::title(StringView t) {
    map_.title = String(t);
    return *this;
}

MapBuilder& MapBuilder::field(StringView name, UInt16 row, UInt16 col, UInt16 length) {
    return field(name, row, col, length, FieldAttribute::UNPROT_NORM);
}

MapBuilder& MapBuilder::field(StringView name, UInt16 row, UInt16 col, UInt16 length,
                              FieldAttribute attr) {
    return field(name, row, col, length, attr, "");
}

MapBuilder& MapBuilder::field(StringView name, UInt16 row, UInt16 col, UInt16 length,
                              FieldAttribute attr, StringView initial) {
    FieldDefinition field;
    field.name = String(name);
    field.row = row;
    field.col = col;
    field.length = length;
    field.attribute = attr;
    field.initial_value = String(initial);
    map_.fields.push_back(field);
    return *this;
}

MapBuilder& MapBuilder::label(UInt16 row, UInt16 col, StringView text) {
    return label(row, col, text, FieldAttribute::PROT_NORM);
}

MapBuilder& MapBuilder::label(UInt16 row, UInt16 col, StringView text, FieldAttribute attr) {
    FieldDefinition field;
    field.name = "_label_" + std::to_string(map_.fields.size());
    field.row = row;
    field.col = col;
    field.length = static_cast<UInt16>(text.length());
    field.attribute = attr;
    field.initial_value = String(text);
    map_.fields.push_back(field);
    return *this;
}

Result<void> MapBuilder::register_map() {
    return BMSManager::instance().define_map(map_);
}

} // namespace bms
} // namespace cics
