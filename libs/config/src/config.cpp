// =============================================================================
// IBM CICS Emulation - Configuration File Parser Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/config/config.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace cics::config {

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

String trim(StringView sv) {
    size_t start = 0;
    while (start < sv.size() && std::isspace(static_cast<unsigned char>(sv[start]))) {
        ++start;
    }
    size_t end = sv.size();
    while (end > start && std::isspace(static_cast<unsigned char>(sv[end - 1]))) {
        --end;
    }
    return String(sv.substr(start, end - start));
}

String to_lower(StringView sv) {
    String result(sv);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool is_true_value(StringView sv) {
    String lower = to_lower(sv);
    return lower == "true" || lower == "yes" || lower == "on" || lower == "1";
}

bool is_false_value(StringView sv) {
    String lower = to_lower(sv);
    return lower == "false" || lower == "no" || lower == "off" || lower == "0";
}

} // anonymous namespace

// =============================================================================
// ConfigValue Implementation
// =============================================================================

Result<Int64> ConfigValue::to_int() const {
    if (value_.empty()) {
        return make_error<Int64>(ErrorCode::INVALID_ARGUMENT, "Empty value");
    }
    try {
        size_t pos;
        Int64 result = std::stoll(value_, &pos);
        if (pos != value_.size()) {
            return make_error<Int64>(ErrorCode::INVALID_ARGUMENT, "Invalid integer format");
        }
        return result;
    } catch (...) {
        return make_error<Int64>(ErrorCode::INVALID_ARGUMENT, "Cannot parse integer");
    }
}

Result<UInt64> ConfigValue::to_uint() const {
    if (value_.empty()) {
        return make_error<UInt64>(ErrorCode::INVALID_ARGUMENT, "Empty value");
    }
    try {
        size_t pos;
        UInt64 result = std::stoull(value_, &pos);
        if (pos != value_.size()) {
            return make_error<UInt64>(ErrorCode::INVALID_ARGUMENT, "Invalid integer format");
        }
        return result;
    } catch (...) {
        return make_error<UInt64>(ErrorCode::INVALID_ARGUMENT, "Cannot parse unsigned integer");
    }
}

Result<Float64> ConfigValue::to_double() const {
    if (value_.empty()) {
        return make_error<Float64>(ErrorCode::INVALID_ARGUMENT, "Empty value");
    }
    try {
        size_t pos;
        Float64 result = std::stod(value_, &pos);
        if (pos != value_.size()) {
            return make_error<Float64>(ErrorCode::INVALID_ARGUMENT, "Invalid number format");
        }
        return result;
    } catch (...) {
        return make_error<Float64>(ErrorCode::INVALID_ARGUMENT, "Cannot parse number");
    }
}

Result<bool> ConfigValue::to_bool() const {
    if (is_true_value(value_)) return true;
    if (is_false_value(value_)) return false;
    return make_error<bool>(ErrorCode::INVALID_ARGUMENT, "Cannot parse boolean");
}

Int64 ConfigValue::to_int_or(Int64 default_val) const {
    auto result = to_int();
    return result.is_success() ? result.value() : default_val;
}

UInt64 ConfigValue::to_uint_or(UInt64 default_val) const {
    auto result = to_uint();
    return result.is_success() ? result.value() : default_val;
}

Float64 ConfigValue::to_double_or(Float64 default_val) const {
    auto result = to_double();
    return result.is_success() ? result.value() : default_val;
}

bool ConfigValue::to_bool_or(bool default_val) const {
    auto result = to_bool();
    return result.is_success() ? result.value() : default_val;
}

String ConfigValue::to_string_or(StringView default_val) const {
    return value_.empty() ? String(default_val) : value_;
}

Vector<String> ConfigValue::to_list(char delimiter) const {
    Vector<String> result;
    std::istringstream iss(value_);
    String item;
    while (std::getline(iss, item, delimiter)) {
        String trimmed = trim(item);
        if (!trimmed.empty()) {
            result.push_back(std::move(trimmed));
        }
    }
    return result;
}

// =============================================================================
// ConfigSection Implementation
// =============================================================================

static const ConfigValue EMPTY_VALUE;

bool ConfigSection::has(StringView key) const {
    return values_.find(key) != values_.end();
}

const ConfigValue& ConfigSection::get(StringView key) const {
    auto it = values_.find(key);
    return it != values_.end() ? it->second : EMPTY_VALUE;
}

ConfigValue ConfigSection::get_or(StringView key, StringView default_val) const {
    auto it = values_.find(key);
    return it != values_.end() ? it->second : ConfigValue(String(default_val));
}

String ConfigSection::get_string(StringView key, StringView default_val) const {
    return get(key).to_string_or(default_val);
}

Int64 ConfigSection::get_int(StringView key, Int64 default_val) const {
    return get(key).to_int_or(default_val);
}

UInt64 ConfigSection::get_uint(StringView key, UInt64 default_val) const {
    return get(key).to_uint_or(default_val);
}

Float64 ConfigSection::get_double(StringView key, Float64 default_val) const {
    return get(key).to_double_or(default_val);
}

bool ConfigSection::get_bool(StringView key, bool default_val) const {
    return get(key).to_bool_or(default_val);
}

Vector<String> ConfigSection::get_list(StringView key, char delimiter) const {
    return get(key).to_list(delimiter);
}

void ConfigSection::set(StringView key, StringView value) {
    values_[String(key)] = ConfigValue(String(value));
}

void ConfigSection::set(StringView key, Int64 value) {
    values_[String(key)] = ConfigValue(std::to_string(value));
}

void ConfigSection::set(StringView key, UInt64 value) {
    values_[String(key)] = ConfigValue(std::to_string(value));
}

void ConfigSection::set(StringView key, Float64 value) {
    values_[String(key)] = ConfigValue(std::to_string(value));
}

void ConfigSection::set(StringView key, bool value) {
    values_[String(key)] = ConfigValue(value ? "true" : "false");
}

void ConfigSection::remove(StringView key) {
    auto it = values_.find(key);
    if (it != values_.end()) {
        values_.erase(it);
    }
}

void ConfigSection::clear() {
    values_.clear();
}

Vector<String> ConfigSection::keys() const {
    Vector<String> result;
    result.reserve(values_.size());
    for (const auto& [key, _] : values_) {
        result.push_back(key);
    }
    return result;
}

// =============================================================================
// ConfigFile Implementation
// =============================================================================

ConfigFile::ConfigFile(const Path& path) {
    (void)load(path);  // Ignore return value in constructor
}

void ConfigFile::parse_line(StringView line, String& current_section) {
    // Skip empty lines and comments
    String trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
        return;
    }
    
    // Check for section header [section]
    if (trimmed[0] == '[' && trimmed.back() == ']') {
        current_section = trim(trimmed.substr(1, trimmed.size() - 2));
        if (!has_section(current_section)) {
            add_section(current_section);
        }
        return;
    }
    
    // Parse key=value or key:value
    size_t eq_pos = trimmed.find('=');
    size_t colon_pos = trimmed.find(':');
    size_t sep_pos = std::min(eq_pos, colon_pos);
    
    if (sep_pos != String::npos) {
        String key = trim(trimmed.substr(0, sep_pos));
        String value = trim(trimmed.substr(sep_pos + 1));
        
        // Remove quotes from value
        if (value.size() >= 2) {
            if ((value.front() == '"' && value.back() == '"') ||
                (value.front() == '\'' && value.back() == '\'')) {
                value = value.substr(1, value.size() - 2);
            }
        }
        
        section(current_section).set(key, value);
    }
}

Result<void> ConfigFile::load(const Path& path) {
    std::ifstream file(path);
    if (!file) {
        return make_error<void>(ErrorCode::FILE_NOT_FOUND, 
            "Cannot open config file: " + path.string());
    }
    
    filepath_ = path;
    sections_.clear();
    
    String current_section = default_section_name_;
    add_section(current_section);
    
    String line;
    while (std::getline(file, line)) {
        parse_line(line, current_section);
    }
    
    modified_ = false;
    return {};
}

Result<void> ConfigFile::save() const {
    if (filepath_.empty()) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "No file path set");
    }
    return save(filepath_);
}

Result<void> ConfigFile::save(const Path& path) const {
    std::ofstream file(path);
    if (!file) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot create config file: " + path.string());
    }
    
    file << to_string();
    return {};
}

Result<void> ConfigFile::reload() {
    if (filepath_.empty()) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "No file loaded");
    }
    return load(filepath_);
}

bool ConfigFile::has_section(StringView name) const {
    return sections_.find(name) != sections_.end();
}

static ConfigSection EMPTY_SECTION;

ConfigSection& ConfigFile::section(StringView name) {
    auto it = sections_.find(name);
    if (it == sections_.end()) {
        return add_section(name);
    }
    return it->second;
}

const ConfigSection& ConfigFile::section(StringView name) const {
    auto it = sections_.find(name);
    return it != sections_.end() ? it->second : EMPTY_SECTION;
}

ConfigSection& ConfigFile::operator[](StringView name) {
    return section(name);
}

const ConfigSection& ConfigFile::operator[](StringView name) const {
    return section(name);
}

ConfigSection& ConfigFile::default_section() {
    return section(default_section_name_);
}

const ConfigSection& ConfigFile::default_section() const {
    return section(default_section_name_);
}

void ConfigFile::set_default_section_name(StringView name) {
    default_section_name_ = String(name);
}

bool ConfigFile::has(StringView key) const {
    if (default_section().has(key)) return true;
    for (const auto& [_, sec] : sections_) {
        if (sec.has(key)) return true;
    }
    return false;
}

bool ConfigFile::has(StringView section_name, StringView key) const {
    return section(section_name).has(key);
}

String ConfigFile::get_string(StringView key, StringView default_val) const {
    if (default_section().has(key)) {
        return default_section().get_string(key, default_val);
    }
    for (const auto& [_, sec] : sections_) {
        if (sec.has(key)) {
            return sec.get_string(key, default_val);
        }
    }
    return String(default_val);
}

String ConfigFile::get_string(StringView section_name, StringView key, StringView default_val) const {
    return section(section_name).get_string(key, default_val);
}

Int64 ConfigFile::get_int(StringView key, Int64 default_val) const {
    if (default_section().has(key)) {
        return default_section().get_int(key, default_val);
    }
    for (const auto& [_, sec] : sections_) {
        if (sec.has(key)) {
            return sec.get_int(key, default_val);
        }
    }
    return default_val;
}

Int64 ConfigFile::get_int(StringView section_name, StringView key, Int64 default_val) const {
    return section(section_name).get_int(key, default_val);
}

bool ConfigFile::get_bool(StringView key, bool default_val) const {
    if (default_section().has(key)) {
        return default_section().get_bool(key, default_val);
    }
    for (const auto& [_, sec] : sections_) {
        if (sec.has(key)) {
            return sec.get_bool(key, default_val);
        }
    }
    return default_val;
}

bool ConfigFile::get_bool(StringView section_name, StringView key, bool default_val) const {
    return section(section_name).get_bool(key, default_val);
}

void ConfigFile::set(StringView key, StringView value) {
    default_section().set(key, value);
    modified_ = true;
}

void ConfigFile::set(StringView section_name, StringView key, StringView value) {
    section(section_name).set(key, value);
    modified_ = true;
}

ConfigSection& ConfigFile::add_section(StringView name) {
    auto [it, _] = sections_.emplace(String(name), ConfigSection(String(name)));
    modified_ = true;
    return it->second;
}

void ConfigFile::remove_section(StringView name) {
    auto it = sections_.find(name);
    if (it != sections_.end()) {
        sections_.erase(it);
        modified_ = true;
    }
}

void ConfigFile::clear() {
    sections_.clear();
    modified_ = true;
}

Vector<String> ConfigFile::section_names() const {
    Vector<String> result;
    result.reserve(sections_.size());
    for (const auto& [name, _] : sections_) {
        result.push_back(name);
    }
    return result;
}

String ConfigFile::to_string() const {
    std::ostringstream oss;
    
    // Write default section first (without header if it's the only section)
    bool has_other_sections = sections_.size() > 1 || 
        (sections_.size() == 1 && !has_section(default_section_name_));
    
    if (has_section(default_section_name_)) {
        const auto& def = section(default_section_name_);
        if (!def.empty()) {
            if (has_other_sections) {
                oss << "[" << default_section_name_ << "]\n";
            }
            for (const auto& [key, value] : def) {
                oss << key << " = " << value.str() << "\n";
            }
            oss << "\n";
        }
    }
    
    // Write other sections
    for (const auto& [name, sec] : sections_) {
        if (name == default_section_name_) continue;
        if (sec.empty()) continue;
        
        oss << "[" << name << "]\n";
        for (const auto& [key, value] : sec) {
            oss << key << " = " << value.str() << "\n";
        }
        oss << "\n";
    }
    
    return oss.str();
}

// =============================================================================
// Environment Variable Support
// =============================================================================

Optional<String> get_env(StringView name) {
    const char* value = std::getenv(String(name).c_str());
    if (value) {
        return String(value);
    }
    return nullopt;
}

String get_env_or(StringView name, StringView default_val) {
    auto value = get_env(name);
    return value.has_value() ? value.value() : String(default_val);
}

Result<void> set_env(StringView name, StringView value) {
#ifdef _WIN32
    if (_putenv_s(String(name).c_str(), String(value).c_str()) != 0) {
        return make_error<void>(ErrorCode::IO_ERROR, "Failed to set environment variable");
    }
#else
    if (setenv(String(name).c_str(), String(value).c_str(), 1) != 0) {
        return make_error<void>(ErrorCode::IO_ERROR, "Failed to set environment variable");
    }
#endif
    return {};
}

Result<void> unset_env(StringView name) {
#ifdef _WIN32
    if (_putenv_s(String(name).c_str(), "") != 0) {
        return make_error<void>(ErrorCode::IO_ERROR, "Failed to unset environment variable");
    }
#else
    if (unsetenv(String(name).c_str()) != 0) {
        return make_error<void>(ErrorCode::IO_ERROR, "Failed to unset environment variable");
    }
#endif
    return {};
}

String expand_env(StringView str) {
    String result;
    result.reserve(str.size());
    
    for (size_t i = 0; i < str.size(); ++i) {
        // Handle ${VAR} syntax
        if (str[i] == '$' && i + 1 < str.size() && str[i + 1] == '{') {
            size_t end = str.find('}', i + 2);
            if (end != StringView::npos) {
                String var_name(str.substr(i + 2, end - i - 2));
                auto value = get_env(var_name);
                if (value) {
                    result += value.value();
                }
                i = end;
                continue;
            }
        }
        // Handle %VAR% syntax (Windows)
        else if (str[i] == '%') {
            size_t end = str.find('%', i + 1);
            if (end != StringView::npos) {
                String var_name(str.substr(i + 1, end - i - 1));
                auto value = get_env(var_name);
                if (value) {
                    result += value.value();
                }
                i = end;
                continue;
            }
        }
        
        result += str[i];
    }
    
    return result;
}

// =============================================================================
// ConfigBuilder Implementation
// =============================================================================

ConfigBuilder::ConfigBuilder() : current_section_("default") {
    config_.add_section(current_section_);
}

ConfigBuilder& ConfigBuilder::section(StringView name) {
    current_section_ = String(name);
    if (!config_.has_section(current_section_)) {
        config_.add_section(current_section_);
    }
    return *this;
}

ConfigBuilder& ConfigBuilder::default_section() {
    current_section_ = "default";
    return *this;
}

ConfigBuilder& ConfigBuilder::set(StringView key, StringView value) {
    config_.section(current_section_).set(key, value);
    return *this;
}

ConfigBuilder& ConfigBuilder::set(StringView key, Int64 value) {
    config_.section(current_section_).set(key, value);
    return *this;
}

ConfigBuilder& ConfigBuilder::set(StringView key, Float64 value) {
    config_.section(current_section_).set(key, value);
    return *this;
}

ConfigBuilder& ConfigBuilder::set(StringView key, bool value) {
    config_.section(current_section_).set(key, value);
    return *this;
}

ConfigFile ConfigBuilder::build() {
    return std::move(config_);
}

Result<void> ConfigBuilder::save(const Path& path) {
    return config_.save(path);
}

// =============================================================================
// Factory Functions
// =============================================================================

Result<ConfigFile> load_config(const Path& path) {
    ConfigFile config;
    auto result = config.load(path);
    if (!result.is_success()) {
        return make_error<ConfigFile>(result.error().code, result.error().message);
    }
    return config;
}

Result<ConfigFile> parse_config(StringView content) {
    ConfigFile config;
    String current_section = "default";
    config.add_section(current_section);
    
    String content_str{content};
    std::istringstream iss{content_str};
    String line;
    while (std::getline(iss, line)) {
        // Parse line directly here instead of calling private method
        String trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }
        
        // Check for section header [section]
        if (trimmed[0] == '[' && trimmed.back() == ']') {
            current_section = trim(trimmed.substr(1, trimmed.size() - 2));
            if (!config.has_section(current_section)) {
                config.add_section(current_section);
            }
            continue;
        }
        
        // Parse key=value or key:value
        size_t eq_pos = trimmed.find('=');
        size_t colon_pos = trimmed.find(':');
        size_t sep_pos = std::min(eq_pos, colon_pos);
        
        if (sep_pos != String::npos) {
            String key = trim(trimmed.substr(0, sep_pos));
            String value = trim(trimmed.substr(sep_pos + 1));
            
            // Remove quotes from value
            if (value.size() >= 2) {
                if ((value.front() == '"' && value.back() == '"') ||
                    (value.front() == '\'' && value.back() == '\'')) {
                    value = value.substr(1, value.size() - 2);
                }
            }
            
            config.section(current_section).set(key, value);
        }
    }
    
    return config;
}

ConfigFile create_config() {
    return ConfigFile();
}

// =============================================================================
// CICS-Specific Configuration
// =============================================================================

Result<ConfigFile> load_cics_config(const Path& path) {
    return load_config(path);
}

ConfigFile default_cics_config() {
    ConfigBuilder builder;
    
    builder.section(sections::SYSTEM)
        .set("name", "CICS_EMULATION")
        .set("version", "3.4.6")
        .set("region_size", static_cast<Int64>(64 * 1024 * 1024))
        .set("max_tasks", static_cast<Int64>(100));
    
    builder.section(sections::VSAM)
        .set("default_ci_size", static_cast<Int64>(4096))
        .set("default_buffers", static_cast<Int64>(4))
        .set("index_buffers", static_cast<Int64>(4));
    
    builder.section(sections::TRANSACTION)
        .set("default_timeout", static_cast<Int64>(30))
        .set("max_transaction_time", static_cast<Int64>(300));
    
    builder.section(sections::LOGGING)
        .set("level", "INFO")
        .set("console_output", true)
        .set("file_output", true)
        .set("log_directory", "logs");
    
    builder.section(sections::SECURITY)
        .set("enabled", false)
        .set("default_user", "CICSUSER");
    
    return builder.build();
}

} // namespace cics::config
