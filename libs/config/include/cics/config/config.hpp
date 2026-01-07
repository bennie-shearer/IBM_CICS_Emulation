#pragma once
// =============================================================================
// IBM CICS Emulation - Configuration File Parser
// Version: 3.4.6
// INI/Config file support for Windows, Linux, and macOS
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <map>

namespace cics::config {

// =============================================================================
// Configuration Value
// =============================================================================

class ConfigValue {
private:
    String value_;
    
public:
    ConfigValue() = default;
    explicit ConfigValue(String value) : value_(std::move(value)) {}
    
    // String access
    [[nodiscard]] const String& str() const { return value_; }
    [[nodiscard]] StringView view() const { return value_; }
    [[nodiscard]] bool empty() const { return value_.empty(); }
    
    // Type conversions
    [[nodiscard]] Result<Int64> to_int() const;
    [[nodiscard]] Result<UInt64> to_uint() const;
    [[nodiscard]] Result<Float64> to_double() const;
    [[nodiscard]] Result<bool> to_bool() const;
    
    // With defaults
    [[nodiscard]] Int64 to_int_or(Int64 default_val) const;
    [[nodiscard]] UInt64 to_uint_or(UInt64 default_val) const;
    [[nodiscard]] Float64 to_double_or(Float64 default_val) const;
    [[nodiscard]] bool to_bool_or(bool default_val) const;
    [[nodiscard]] String to_string_or(StringView default_val) const;
    
    // List parsing (comma-separated)
    [[nodiscard]] Vector<String> to_list(char delimiter = ',') const;
    
    // Implicit conversion
    operator String() const { return value_; }
    operator StringView() const { return value_; }
};

// =============================================================================
// Configuration Section
// =============================================================================

class ConfigSection {
private:
    String name_;
    std::map<String, ConfigValue, std::less<>> values_;
    
public:
    ConfigSection() = default;
    explicit ConfigSection(String name) : name_(std::move(name)) {}
    
    // Section name
    [[nodiscard]] const String& name() const { return name_; }
    
    // Value access
    [[nodiscard]] bool has(StringView key) const;
    [[nodiscard]] const ConfigValue& get(StringView key) const;
    [[nodiscard]] ConfigValue get_or(StringView key, StringView default_val) const;
    
    // Typed access with defaults
    [[nodiscard]] String get_string(StringView key, StringView default_val = "") const;
    [[nodiscard]] Int64 get_int(StringView key, Int64 default_val = 0) const;
    [[nodiscard]] UInt64 get_uint(StringView key, UInt64 default_val = 0) const;
    [[nodiscard]] Float64 get_double(StringView key, Float64 default_val = 0.0) const;
    [[nodiscard]] bool get_bool(StringView key, bool default_val = false) const;
    [[nodiscard]] Vector<String> get_list(StringView key, char delimiter = ',') const;
    
    // Modification
    void set(StringView key, StringView value);
    void set(StringView key, Int64 value);
    void set(StringView key, UInt64 value);
    void set(StringView key, Float64 value);
    void set(StringView key, bool value);
    void remove(StringView key);
    void clear();
    
    // Iteration
    [[nodiscard]] Vector<String> keys() const;
    [[nodiscard]] Size size() const { return values_.size(); }
    [[nodiscard]] bool empty() const { return values_.empty(); }
    
    // Iterator support
    auto begin() { return values_.begin(); }
    auto end() { return values_.end(); }
    auto begin() const { return values_.begin(); }
    auto end() const { return values_.end(); }
};

// =============================================================================
// Configuration File
// =============================================================================

class ConfigFile {
private:
    Path filepath_;
    String default_section_name_ = "default";
    std::map<String, ConfigSection, std::less<>> sections_;
    Vector<String> comments_;
    bool modified_ = false;
    
    // Parser state
    void parse_line(StringView line, String& current_section);
    
public:
    ConfigFile() = default;
    explicit ConfigFile(const Path& path);
    
    // File operations
    [[nodiscard]] Result<void> load(const Path& path);
    [[nodiscard]] Result<void> save() const;
    [[nodiscard]] Result<void> save(const Path& path) const;
    [[nodiscard]] Result<void> reload();
    
    // Path info
    [[nodiscard]] const Path& path() const { return filepath_; }
    [[nodiscard]] bool is_loaded() const { return !filepath_.empty(); }
    [[nodiscard]] bool is_modified() const { return modified_; }
    
    // Section access
    [[nodiscard]] bool has_section(StringView name) const;
    [[nodiscard]] ConfigSection& section(StringView name);
    [[nodiscard]] const ConfigSection& section(StringView name) const;
    [[nodiscard]] ConfigSection& operator[](StringView name);
    [[nodiscard]] const ConfigSection& operator[](StringView name) const;
    
    // Default section (for INI files without explicit sections)
    [[nodiscard]] ConfigSection& default_section();
    [[nodiscard]] const ConfigSection& default_section() const;
    void set_default_section_name(StringView name);
    
    // Quick access (searches default section first, then all sections)
    [[nodiscard]] bool has(StringView key) const;
    [[nodiscard]] bool has(StringView section, StringView key) const;
    [[nodiscard]] String get_string(StringView key, StringView default_val = "") const;
    [[nodiscard]] String get_string(StringView section, StringView key, StringView default_val = "") const;
    [[nodiscard]] Int64 get_int(StringView key, Int64 default_val = 0) const;
    [[nodiscard]] Int64 get_int(StringView section, StringView key, Int64 default_val = 0) const;
    [[nodiscard]] bool get_bool(StringView key, bool default_val = false) const;
    [[nodiscard]] bool get_bool(StringView section, StringView key, bool default_val = false) const;
    
    // Modification
    void set(StringView key, StringView value);
    void set(StringView section, StringView key, StringView value);
    ConfigSection& add_section(StringView name);
    void remove_section(StringView name);
    void clear();
    
    // Iteration
    [[nodiscard]] Vector<String> section_names() const;
    [[nodiscard]] Size section_count() const { return sections_.size(); }
    
    // Iterator support
    auto begin() { return sections_.begin(); }
    auto end() { return sections_.end(); }
    auto begin() const { return sections_.begin(); }
    auto end() const { return sections_.end(); }
    
    // Serialization
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// Environment Variable Support
// =============================================================================

// Get environment variable
[[nodiscard]] Optional<String> get_env(StringView name);

// Get with default
[[nodiscard]] String get_env_or(StringView name, StringView default_val);

// Set environment variable
[[nodiscard]] Result<void> set_env(StringView name, StringView value);

// Unset environment variable
[[nodiscard]] Result<void> unset_env(StringView name);

// Expand environment variables in string (${VAR} or %VAR%)
[[nodiscard]] String expand_env(StringView str);

// =============================================================================
// Configuration Builder (Fluent API)
// =============================================================================

class ConfigBuilder {
private:
    ConfigFile config_;
    String current_section_;
    
public:
    ConfigBuilder();
    
    // Section selection
    ConfigBuilder& section(StringView name);
    ConfigBuilder& default_section();
    
    // Value setting
    ConfigBuilder& set(StringView key, StringView value);
    ConfigBuilder& set(StringView key, Int64 value);
    ConfigBuilder& set(StringView key, Float64 value);
    ConfigBuilder& set(StringView key, bool value);
    
    // Build and save
    [[nodiscard]] ConfigFile build();
    [[nodiscard]] Result<void> save(const Path& path);
};

// =============================================================================
// Factory Functions
// =============================================================================

// Load configuration from file
[[nodiscard]] Result<ConfigFile> load_config(const Path& path);

// Load configuration from string
[[nodiscard]] Result<ConfigFile> parse_config(StringView content);

// Create empty configuration
[[nodiscard]] ConfigFile create_config();

// =============================================================================
// CICS-Specific Configuration
// =============================================================================

// Standard CICS configuration sections
namespace sections {
    constexpr StringView SYSTEM = "SYSTEM";
    constexpr StringView VSAM = "VSAM";
    constexpr StringView TRANSACTION = "TRANSACTION";
    constexpr StringView PROGRAM = "PROGRAM";
    constexpr StringView FILE = "FILE";
    constexpr StringView SECURITY = "SECURITY";
    constexpr StringView LOGGING = "LOGGING";
    constexpr StringView PERFORMANCE = "PERFORMANCE";
}

// Load CICS system configuration
[[nodiscard]] Result<ConfigFile> load_cics_config(const Path& path);

// Get default CICS configuration
[[nodiscard]] ConfigFile default_cics_config();

} // namespace cics::config
