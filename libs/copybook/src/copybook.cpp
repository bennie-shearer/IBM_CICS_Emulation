// =============================================================================
// CICS Emulation - Copybook Parser Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/copybook/copybook.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <cstring>

namespace cics {
namespace copybook {

// =============================================================================
// PictureClause Implementation
// =============================================================================

UInt16 PictureClause::storage_size() const {
    switch (type) {
        case DataType::ALPHANUMERIC:
        case DataType::ALPHABETIC:
        case DataType::NUMERIC_DISPLAY:
            return static_cast<UInt16>(total_digits + (is_signed && sign_position == 'S' ? 1 : 0));
        case DataType::NUMERIC_PACKED:
            return static_cast<UInt16>((total_digits + 2) / 2);
        case DataType::NUMERIC_BINARY:
            return comp_storage_size(total_digits);
        case DataType::NUMERIC_FLOAT:
            return decimal_digits > 0 ? 8 : 4;  // COMP-2 vs COMP-1
        default:
            return total_digits;
    }
}

String PictureClause::to_cpp_type() const {
    switch (type) {
        case DataType::ALPHANUMERIC:
        case DataType::ALPHABETIC:
            return "std::array<char, " + std::to_string(total_digits) + ">";
        case DataType::NUMERIC_DISPLAY:
        case DataType::NUMERIC_PACKED:
            if (has_decimal) {
                return "double";
            }
            return total_digits <= 9 ? "int32_t" : "int64_t";
        case DataType::NUMERIC_BINARY:
            if (total_digits <= 4) return is_signed ? "int16_t" : "uint16_t";
            if (total_digits <= 9) return is_signed ? "int32_t" : "uint32_t";
            return is_signed ? "int64_t" : "uint64_t";
        case DataType::NUMERIC_FLOAT:
            return decimal_digits > 0 ? "double" : "float";
        default:
            return "uint8_t";
    }
}

String PictureClause::to_string() const {
    std::ostringstream oss;
    oss << "PIC " << raw_picture;
    if (is_signed) oss << " (signed)";
    if (has_decimal) oss << " V" << decimal_digits;
    oss << " [" << storage_size() << " bytes]";
    return oss.str();
}

Result<PictureClause> PictureClause::parse(StringView pic) {
    PictureClause result;
    result.raw_picture = String(pic);
    
    String upper_pic;
    for (char c : pic) {
        upper_pic += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    
    size_t pos = 0;
    bool after_v = false;
    
    while (pos < upper_pic.length()) {
        char c = upper_pic[pos];
        UInt16 count = 1;
        
        // Check for repetition
        if (pos + 1 < upper_pic.length() && upper_pic[pos + 1] == '(') {
            size_t end = upper_pic.find(')', pos + 2);
            if (end != String::npos) {
                count = static_cast<UInt16>(std::stoi(upper_pic.substr(pos + 2, end - pos - 2)));
                pos = end + 1;
            } else {
                ++pos;
            }
        } else {
            ++pos;
        }
        
        switch (c) {
            case 'X':
                result.type = DataType::ALPHANUMERIC;
                result.total_digits += count;
                break;
            case 'A':
                result.type = DataType::ALPHABETIC;
                result.total_digits += count;
                break;
            case '9':
                if (result.type == DataType::ALPHANUMERIC) {
                    result.type = DataType::NUMERIC_DISPLAY;
                }
                if (after_v) {
                    result.decimal_digits += count;
                }
                result.total_digits += count;
                break;
            case 'S':
                result.is_signed = true;
                result.sign_position = 'L';
                break;
            case 'V':
                result.has_decimal = true;
                after_v = true;
                break;
            case 'P':
                // Implied decimal position
                result.total_digits += count;
                break;
            case 'Z':
            case '*':
            case '$':
            case ',':
            case '.':
            case '-':
            case '+':
                // Editing characters
                result.total_digits += count;
                break;
            default:
                break;
        }
    }
    
    if (result.total_digits == 0) {
        result.total_digits = 1;
    }
    
    return make_success(result);
}

// =============================================================================
// CopybookField Implementation
// =============================================================================

UInt32 CopybookField::total_size() const {
    UInt32 base_size = size;
    if (occurs > 0) {
        base_size *= occurs;
    }
    return base_size;
}

String CopybookField::to_string() const {
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << static_cast<int>(level) << " " << name;
    if (!picture.raw_picture.empty()) {
        oss << " PIC " << picture.raw_picture;
    }
    if (occurs > 0) {
        oss << " OCCURS " << occurs;
    }
    oss << " [offset=" << offset << ", size=" << size << "]";
    return oss.str();
}

String CopybookField::to_cpp_declaration() const {
    std::ostringstream oss;
    
    if (is_group()) {
        oss << "struct " << cobol_to_cpp_name(name) << " {\n";
        for (const auto& child : children) {
            oss << "    " << child->to_cpp_declaration() << "\n";
        }
        oss << "}";
    } else {
        String cpp_type = picture.to_cpp_type();
        if (occurs > 0) {
            oss << "std::array<" << cpp_type << ", " << occurs << "> " 
                << cobol_to_cpp_name(name);
        } else {
            oss << cpp_type << " " << cobol_to_cpp_name(name);
        }
    }
    oss << ";";
    
    return oss.str();
}

// =============================================================================
// CopybookDefinition Implementation
// =============================================================================

const CopybookField* CopybookDefinition::find_field(StringView field_name) const {
    std::function<const CopybookField*(const std::vector<std::unique_ptr<CopybookField>>&)> search;
    search = [&](const std::vector<std::unique_ptr<CopybookField>>& flds) -> const CopybookField* {
        for (const auto& field : flds) {
            if (field->name == field_name) {
                return field.get();
            }
            if (!field->children.empty()) {
                if (auto found = search(field->children)) {
                    return found;
                }
            }
        }
        return nullptr;
    };
    return search(fields);
}

std::vector<const CopybookField*> CopybookDefinition::get_all_fields() const {
    std::vector<const CopybookField*> result;
    std::function<void(const std::vector<std::unique_ptr<CopybookField>>&)> collect;
    collect = [&](const std::vector<std::unique_ptr<CopybookField>>& flds) {
        for (const auto& field : flds) {
            result.push_back(field.get());
            if (!field->children.empty()) {
                collect(field->children);
            }
        }
    };
    collect(fields);
    return result;
}

String CopybookDefinition::to_string() const {
    std::ostringstream oss;
    oss << "Copybook: " << name << " (length=" << record_length << ")\n";
    
    std::function<void(const std::vector<std::unique_ptr<CopybookField>>&, int)> print;
    print = [&](const std::vector<std::unique_ptr<CopybookField>>& flds, int indent) {
        for (const auto& field : flds) {
            oss << String(indent * 2, ' ') << field->to_string() << "\n";
            if (!field->children.empty()) {
                print(field->children, indent + 1);
            }
        }
    };
    print(fields, 0);
    
    return oss.str();
}

String CopybookDefinition::to_cpp_struct() const {
    std::ostringstream oss;
    
    oss << "#pragma pack(push, 1)\n";
    oss << "struct " << cobol_to_cpp_name(name) << " {\n";
    
    std::function<void(const std::vector<std::unique_ptr<CopybookField>>&, int)> generate;
    generate = [&](const std::vector<std::unique_ptr<CopybookField>>& flds, int indent) {
        for (const auto& field : flds) {
            if (field->level == 88) continue;  // Skip conditions
            
            String spaces(indent * 4, ' ');
            
            if (field->is_group()) {
                oss << spaces << "struct {\n";
                generate(field->children, indent + 1);
                oss << spaces << "} " << cobol_to_cpp_name(field->name);
                if (field->occurs > 0) {
                    oss << "[" << field->occurs << "]";
                }
                oss << ";\n";
            } else {
                // Elementary item
                String cpp_type;
                if (field->picture.type == DataType::ALPHANUMERIC ||
                    field->picture.type == DataType::ALPHABETIC) {
                    cpp_type = "char";
                    oss << spaces << cpp_type << " " << cobol_to_cpp_name(field->name);
                    oss << "[" << field->size << "]";
                } else {
                    cpp_type = field->picture.to_cpp_type();
                    oss << spaces << cpp_type << " " << cobol_to_cpp_name(field->name);
                }
                if (field->occurs > 0) {
                    oss << "[" << field->occurs << "]";
                }
                oss << ";\n";
            }
        }
    };
    
    generate(fields, 1);
    
    oss << "};\n";
    oss << "#pragma pack(pop)\n";
    oss << "static_assert(sizeof(" << cobol_to_cpp_name(name) << ") == " 
        << record_length << ", \"Record size mismatch\");\n";
    
    return oss.str();
}

String CopybookDefinition::to_cpp_header() const {
    std::ostringstream oss;
    
    String guard = cobol_to_cpp_name(name) + "_HPP";
    std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);
    
    oss << "#ifndef " << guard << "\n";
    oss << "#define " << guard << "\n\n";
    oss << "#include <cstdint>\n";
    oss << "#include <array>\n";
    oss << "#include <cstring>\n\n";
    oss << to_cpp_struct();
    oss << "\n#endif // " << guard << "\n";
    
    return oss.str();
}

// =============================================================================
// CopybookParser Implementation
// =============================================================================

void CopybookParser::skip_whitespace() {
    while (!at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\n') {
            advance();
            ++line_;
            column_ = 1;
        } else if (c == '*' && column_ == 7) {
            // Comment line
            skip_to_next_line();
        } else {
            break;
        }
    }
}

void CopybookParser::skip_to_next_line() {
    while (!at_end() && peek() != '\n') {
        advance();
    }
    if (!at_end()) {
        advance();
        ++line_;
        column_ = 1;
    }
}

bool CopybookParser::at_end() const {
    return position_ >= source_.length();
}

char CopybookParser::peek() const {
    if (at_end()) return '\0';
    return source_[position_];
}

char CopybookParser::advance() {
    char c = source_[position_++];
    ++column_;
    return c;
}

bool CopybookParser::match(char c) {
    if (at_end() || peek() != c) return false;
    advance();
    return true;
}

bool CopybookParser::match(StringView str) {
    if (position_ + str.length() > source_.length()) return false;
    for (size_t i = 0; i < str.length(); ++i) {
        if (std::toupper(source_[position_ + i]) != std::toupper(str[i])) {
            return false;
        }
    }
    position_ += str.length();
    column_ += static_cast<size_t>(str.length());
    return true;
}

String CopybookParser::read_word() {
    skip_whitespace();
    String word;
    while (!at_end()) {
        char c = peek();
        if (std::isalnum(c) || c == '-' || c == '_') {
            word += advance();
        } else {
            break;
        }
    }
    return word;
}

String CopybookParser::read_picture() {
    skip_whitespace();
    String pic;
    while (!at_end()) {
        char c = peek();
        if (std::isalnum(c) || c == '(' || c == ')' || c == 'V' || c == 'S' ||
            c == '.' || c == ',' || c == '+' || c == '-' || c == '*' ||
            c == 'Z' || c == '$' || c == 'P') {
            pic += advance();
        } else {
            break;
        }
    }
    return pic;
}

Result<CopybookDefinition> CopybookParser::parse(StringView source) {
    source_ = String(source);
    position_ = 0;
    line_ = 1;
    column_ = 1;
    errors_.clear();
    
    CopybookDefinition copybook;
    std::vector<CopybookField*> level_stack;
    
    while (!at_end()) {
        skip_whitespace();
        if (at_end()) break;
        
        // Try to parse a field
        auto level_result = parse_level();
        if (!level_result.is_success()) {
            skip_to_next_line();
            continue;
        }
        
        UInt8 level = level_result.value();
        
        auto name_result = parse_name();
        if (!name_result.is_success()) {
            skip_to_next_line();
            continue;
        }
        
        auto field = std::make_unique<CopybookField>();
        field->level = level;
        field->name = name_result.value();
        
        // Parse optional clauses
        skip_whitespace();
        while (!at_end() && peek() != '.') {
            String word = read_word();
            String upper_word = word;
            std::transform(upper_word.begin(), upper_word.end(), upper_word.begin(), ::toupper);
            
            if (upper_word == "PIC" || upper_word == "PICTURE") {
                skip_whitespace();
                if (match("IS")) skip_whitespace();
                String pic = read_picture();
                auto pic_result = PictureClause::parse(pic);
                if (pic_result.is_success()) {
                    field->picture = pic_result.value();
                    field->size = field->picture.storage_size();
                }
            } else if (upper_word == "COMP" || upper_word == "BINARY" || upper_word == "COMP-4") {
                field->usage = UsageClause::COMP;
                field->picture.type = DataType::NUMERIC_BINARY;
                field->size = comp_storage_size(field->picture.total_digits);
            } else if (upper_word == "COMP-1") {
                field->usage = UsageClause::COMP_1;
                field->picture.type = DataType::NUMERIC_FLOAT;
                field->size = 4;
            } else if (upper_word == "COMP-2") {
                field->usage = UsageClause::COMP_2;
                field->picture.type = DataType::NUMERIC_FLOAT;
                field->size = 8;
            } else if (upper_word == "COMP-3" || upper_word == "PACKED-DECIMAL") {
                field->usage = UsageClause::COMP_3;
                field->picture.type = DataType::NUMERIC_PACKED;
                field->size = static_cast<UInt16>((field->picture.total_digits + 2) / 2);
            } else if (upper_word == "OCCURS") {
                skip_whitespace();
                String count = read_word();
                field->occurs = static_cast<UInt16>(std::stoi(count));
                skip_whitespace();
                if (match("TIMES")) skip_whitespace();
            } else if (upper_word == "REDEFINES") {
                skip_whitespace();
                field->redefines = read_word();
            } else if (upper_word == "VALUE") {
                skip_whitespace();
                if (match("IS")) skip_whitespace();
                // Read value
                if (peek() == '\'' || peek() == '"') {
                    char quote = advance();
                    String val;
                    while (!at_end() && peek() != quote) {
                        val += advance();
                    }
                    if (!at_end()) advance();
                    field->value = val;
                } else {
                    field->value = read_word();
                }
            }
            
            skip_whitespace();
        }
        
        // Skip period
        match('.');
        
        // If no size set and no PIC, it's a group
        if (field->size == 0 && field->picture.raw_picture.empty()) {
            field->picture.type = DataType::GROUP;
        }
        
        // Add to hierarchy
        while (!level_stack.empty() && level_stack.back()->level >= level) {
            level_stack.pop_back();
        }
        
        CopybookField* field_ptr = field.get();
        
        if (level_stack.empty()) {
            copybook.fields.push_back(std::move(field));
        } else {
            field_ptr->parent = level_stack.back();
            level_stack.back()->children.push_back(std::move(field));
        }
        
        level_stack.push_back(field_ptr);
    }
    
    // Calculate offsets and sizes
    calculate_offsets(copybook);
    
    return make_success(std::move(copybook));
}

Result<CopybookDefinition> CopybookParser::parse_file(StringView filename) {
    std::ifstream file{String(filename)};
    if (!file) {
        return make_error<CopybookDefinition>(ErrorCode::FILE_NOT_FOUND,
            "Cannot open file: " + String(filename));
    }
    
    std::ostringstream ss;
    ss << file.rdbuf();
    
    auto result = parse(ss.str());
    if (result.is_success()) {
        result.value().source_file = String(filename);
    }
    return result;
}

Result<UInt8> CopybookParser::parse_level() {
    skip_whitespace();
    String level_str;
    while (!at_end() && std::isdigit(peek())) {
        level_str += advance();
    }
    if (level_str.empty()) {
        return make_error<UInt8>(ErrorCode::INVALID_ARGUMENT, "Expected level number");
    }
    return make_success(static_cast<UInt8>(std::stoi(level_str)));
}

Result<String> CopybookParser::parse_name() {
    skip_whitespace();
    return make_success(read_word());
}

void CopybookParser::calculate_offsets(CopybookDefinition& copybook) {
    UInt32 offset = 0;
    
    std::function<void(std::vector<std::unique_ptr<CopybookField>>&)> calc;
    calc = [&](std::vector<std::unique_ptr<CopybookField>>& fields) {
        for (auto& field : fields) {
            if (!field->redefines.empty()) {
                // REDEFINES - use same offset as redefined field
                if (auto redef = copybook.find_field(field->redefines)) {
                    field->offset = redef->offset;
                }
            } else {
                field->offset = offset;
            }
            
            if (field->is_group()) {
                UInt32 group_start = offset;
                calc(field->children);
                field->size = static_cast<UInt16>(offset - group_start);
            }
            
            if (field->redefines.empty()) {
                offset += field->total_size();
            }
        }
    };
    
    calc(copybook.fields);
    copybook.record_length = offset;
}

// =============================================================================
// CppGenerator Implementation
// =============================================================================

String CppGenerator::generate_header(const CopybookDefinition& copybook) const {
    return copybook.to_cpp_header();
}

String CppGenerator::generate_source(const CopybookDefinition& copybook) const {
    std::ostringstream oss;
    oss << "// Generated from " << copybook.source_file << "\n\n";
    oss << "#include \"" << cobol_to_cpp_name(copybook.name) << ".hpp\"\n\n";
    return oss.str();
}

Result<void> CppGenerator::write_header(const CopybookDefinition& copybook, 
                                         StringView path) const {
    std::ofstream file{String(path)};
    if (!file) {
        return make_error<void>(ErrorCode::FILE_NOT_FOUND,
            "Cannot write file: " + String(path));
    }
    file << generate_header(copybook);
    return make_success();
}

Result<void> CppGenerator::write_source(const CopybookDefinition& copybook,
                                         StringView path) const {
    std::ofstream file{String(path)};
    if (!file) {
        return make_error<void>(ErrorCode::FILE_NOT_FOUND,
            "Cannot write file: " + String(path));
    }
    file << generate_source(copybook);
    return make_success();
}

// =============================================================================
// RecordAccessor Implementation
// =============================================================================

RecordAccessor::RecordAccessor(const CopybookDefinition& copybook, void* data, UInt32 length)
    : copybook_(copybook), data_(static_cast<Byte*>(data)), length_(length) {
}

Result<String> RecordAccessor::get_string(StringView field_name) const {
    const CopybookField* field = copybook_.find_field(field_name);
    if (!field) {
        return make_error<String>(ErrorCode::RECORD_NOT_FOUND,
            "Field not found: " + String(field_name));
    }
    
    if (field->offset + field->size > length_) {
        return make_error<String>(ErrorCode::INVALID_ARGUMENT, "Field out of bounds");
    }
    
    String result(reinterpret_cast<char*>(data_ + field->offset), field->size);
    // Trim trailing spaces
    result.erase(std::find_if(result.rbegin(), result.rend(),
        [](unsigned char ch) { return !std::isspace(ch); }).base(), result.end());
    
    return make_success(result);
}

Result<Int64> RecordAccessor::get_integer(StringView field_name) const {
    const CopybookField* field = copybook_.find_field(field_name);
    if (!field) {
        return make_error<Int64>(ErrorCode::RECORD_NOT_FOUND,
            "Field not found: " + String(field_name));
    }
    
    // Simple implementation - would need proper conversion based on type
    String str = String(reinterpret_cast<char*>(data_ + field->offset), field->size);
    try {
        return make_success(static_cast<Int64>(std::stoll(str)));
    } catch (...) {
        return make_error<Int64>(ErrorCode::INVALID_ARGUMENT, "Invalid numeric value");
    }
}

Result<double> RecordAccessor::get_decimal(StringView field_name) const {
    const CopybookField* field = copybook_.find_field(field_name);
    if (!field) {
        return make_error<double>(ErrorCode::RECORD_NOT_FOUND,
            "Field not found: " + String(field_name));
    }
    
    String str = String(reinterpret_cast<char*>(data_ + field->offset), field->size);
    try {
        return make_success(std::stod(str));
    } catch (...) {
        return make_error<double>(ErrorCode::INVALID_ARGUMENT, "Invalid decimal value");
    }
}

Result<ByteBuffer> RecordAccessor::get_raw(StringView field_name) const {
    const CopybookField* field = copybook_.find_field(field_name);
    if (!field) {
        return make_error<ByteBuffer>(ErrorCode::RECORD_NOT_FOUND,
            "Field not found: " + String(field_name));
    }
    
    ByteBuffer result(data_ + field->offset, data_ + field->offset + field->size);
    return make_success(result);
}

Result<void> RecordAccessor::set_string(StringView field_name, StringView value) {
    const CopybookField* field = copybook_.find_field(field_name);
    if (!field) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Field not found: " + String(field_name));
    }
    
    UInt16 copy_len = std::min(static_cast<UInt16>(value.length()), field->size);
    std::memcpy(data_ + field->offset, value.data(), copy_len);
    
    // Pad with spaces
    if (copy_len < field->size) {
        std::memset(data_ + field->offset + copy_len, ' ', field->size - copy_len);
    }
    
    return make_success();
}

Result<void> RecordAccessor::set_integer(StringView field_name, Int64 value) {
    const CopybookField* field = copybook_.find_field(field_name);
    if (!field) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Field not found: " + String(field_name));
    }
    
    String str = std::to_string(value);
    while (str.length() < field->size) {
        str = "0" + str;
    }
    if (str.length() > field->size) {
        str = str.substr(str.length() - field->size);
    }
    
    std::memcpy(data_ + field->offset, str.data(), field->size);
    return make_success();
}

Result<void> RecordAccessor::set_decimal(StringView field_name, double value) {
    return set_integer(field_name, static_cast<Int64>(value));
}

Result<void> RecordAccessor::set_raw(StringView field_name, const ByteBuffer& value) {
    const CopybookField* field = copybook_.find_field(field_name);
    if (!field) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Field not found: " + String(field_name));
    }
    
    UInt16 copy_len = std::min(static_cast<UInt16>(value.size()), field->size);
    std::memcpy(data_ + field->offset, value.data(), copy_len);
    
    return make_success();
}

Result<String> RecordAccessor::get_string(StringView field_name, UInt16 index) const {
    const CopybookField* field = copybook_.find_field(field_name);
    if (!field || !field->is_array() || index >= field->occurs) {
        return make_error<String>(ErrorCode::INVALID_ARGUMENT, "Invalid array access");
    }
    
    UInt32 element_offset = field->offset + (index * field->size);
    String result(reinterpret_cast<char*>(data_ + element_offset), field->size);
    return make_success(result);
}

Result<void> RecordAccessor::set_string(StringView field_name, UInt16 index, StringView value) {
    const CopybookField* field = copybook_.find_field(field_name);
    if (!field || !field->is_array() || index >= field->occurs) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Invalid array access");
    }
    
    UInt32 element_offset = field->offset + (index * field->size);
    UInt16 copy_len = std::min(static_cast<UInt16>(value.length()), field->size);
    std::memcpy(data_ + element_offset, value.data(), copy_len);
    
    return make_success();
}

void RecordAccessor::clear() {
    std::memset(data_, ' ', length_);
}

String RecordAccessor::dump() const {
    std::ostringstream oss;
    oss << "Record Dump (" << length_ << " bytes):\n";
    
    for (const auto* field : copybook_.get_all_fields()) {
        if (field->is_elementary()) {
            auto result = get_string(field->name);
            if (result.is_success()) {
                oss << "  " << std::setw(20) << std::left << field->name
                    << ": [" << result.value() << "]\n";
            }
        }
    }
    
    return oss.str();
}

// =============================================================================
// Utility Functions
// =============================================================================

String cobol_to_cpp_name(StringView cobol_name) {
    String result;
    bool capitalize_next = false;
    
    for (char c : cobol_name) {
        if (c == '-') {
            capitalize_next = true;
        } else if (capitalize_next) {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            capitalize_next = false;
        } else {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    
    return result;
}

UInt16 comp_storage_size(UInt16 digits) {
    if (digits <= 4) return 2;
    if (digits <= 9) return 4;
    return 8;
}

UInt16 parse_pic_count(StringView pic) {
    auto pos = pic.find('(');
    if (pos == StringView::npos) return 1;
    
    auto end_pos = pic.find(')', pos);
    if (end_pos == StringView::npos) return 1;
    
    String count_str(pic.substr(pos + 1, end_pos - pos - 1));
    return static_cast<UInt16>(std::stoi(count_str));
}

} // namespace copybook
} // namespace cics
