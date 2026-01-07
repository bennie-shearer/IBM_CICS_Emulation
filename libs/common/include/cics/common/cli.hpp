// =============================================================================
// CICS Emulation - Command Line Argument Parser
// Version: 3.4.6
// =============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <functional>

namespace cics::cli {

/**
 * @brief Command-line argument parser
 * 
 * Provides simple, dependency-free argument parsing with support for:
 * - Long options (--name, --name=value)
 * - Short options (-n, -n value)
 * - Boolean flags
 * - Positional arguments
 * - Help generation
 */
class ArgParser {
public:
    struct Option {
        std::string long_name;
        char short_name = 0;
        std::string description;
        std::string default_value;
        bool is_flag = false;
        bool required = false;
    };

    explicit ArgParser(const std::string& program_name = "",
                       const std::string& description = "")
        : program_name_(program_name), description_(description) {}

    /**
     * @brief Add a string option
     */
    ArgParser& add_option(const std::string& long_name,
                          char short_name = 0,
                          const std::string& description = "",
                          const std::string& default_value = "",
                          bool required = false) {
        Option opt;
        opt.long_name = long_name;
        opt.short_name = short_name;
        opt.description = description;
        opt.default_value = default_value;
        opt.is_flag = false;
        opt.required = required;
        options_.push_back(opt);
        return *this;
    }

    /**
     * @brief Add a boolean flag
     */
    ArgParser& add_flag(const std::string& long_name,
                        char short_name = 0,
                        const std::string& description = "") {
        Option opt;
        opt.long_name = long_name;
        opt.short_name = short_name;
        opt.description = description;
        opt.is_flag = true;
        options_.push_back(opt);
        return *this;
    }

    /**
     * @brief Add a positional argument
     */
    ArgParser& add_positional(const std::string& name,
                              const std::string& description = "",
                              bool required = true) {
        positional_names_.push_back(name);
        positional_descriptions_.push_back(description);
        positional_required_.push_back(required);
        return *this;
    }

    /**
     * @brief Parse command-line arguments
     * @return true if parsing succeeded
     */
    bool parse(int argc, char* argv[]) {
        if (argc > 0) {
            if (program_name_.empty()) {
                program_name_ = argv[0];
            }
        }

        // Initialize defaults
        for (const auto& opt : options_) {
            if (!opt.default_value.empty()) {
                values_[opt.long_name] = opt.default_value;
            }
            if (opt.is_flag) {
                flags_[opt.long_name] = false;
            }
        }

        size_t pos_idx = 0;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                show_help();
                return false;
            }

            if (arg.starts_with("--")) {
                // Long option
                std::string name;
                std::string value;
                
                auto eq_pos = arg.find('=');
                if (eq_pos != std::string::npos) {
                    name = arg.substr(2, eq_pos - 2);
                    value = arg.substr(eq_pos + 1);
                } else {
                    name = arg.substr(2);
                }

                auto* opt = find_option(name);
                if (!opt) {
                    error_ = "Unknown option: --" + name;
                    return false;
                }

                if (opt->is_flag) {
                    flags_[opt->long_name] = true;
                } else {
                    if (value.empty()) {
                        if (i + 1 < argc) {
                            value = argv[++i];
                        } else {
                            error_ = "Option --" + name + " requires a value";
                            return false;
                        }
                    }
                    values_[opt->long_name] = value;
                }
            } else if (arg.starts_with("-") && arg.length() > 1) {
                // Short option(s)
                for (size_t j = 1; j < arg.length(); ++j) {
                    char c = arg[j];
                    auto* opt = find_option(c);
                    if (!opt) {
                        error_ = "Unknown option: -";
                        error_ += c;
                        return false;
                    }

                    if (opt->is_flag) {
                        flags_[opt->long_name] = true;
                    } else {
                        // Value follows
                        std::string value;
                        if (j + 1 < arg.length()) {
                            // Rest of arg is value
                            value = arg.substr(j + 1);
                            j = arg.length();
                        } else if (i + 1 < argc) {
                            value = argv[++i];
                        } else {
                            error_ = "Option -";
                            error_ += c;
                            error_ += " requires a value";
                            return false;
                        }
                        values_[opt->long_name] = value;
                    }
                }
            } else {
                // Positional argument
                if (pos_idx < positional_names_.size()) {
                    positional_values_.push_back(arg);
                    ++pos_idx;
                } else {
                    extra_args_.push_back(arg);
                }
            }
        }

        // Check required options
        for (const auto& opt : options_) {
            if (opt.required && values_.find(opt.long_name) == values_.end()) {
                error_ = "Required option missing: --" + opt.long_name;
                return false;
            }
        }

        // Check required positional arguments
        for (size_t i = 0; i < positional_required_.size(); ++i) {
            if (positional_required_[i] && i >= positional_values_.size()) {
                error_ = "Required argument missing: " + positional_names_[i];
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Get string option value
     */
    std::optional<std::string> get(const std::string& name) const {
        auto it = values_.find(name);
        if (it != values_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief Get string option value with default
     */
    std::string get(const std::string& name, const std::string& default_val) const {
        return get(name).value_or(default_val);
    }

    /**
     * @brief Get integer option value
     */
    std::optional<int64_t> get_int(const std::string& name) const {
        auto str = get(name);
        if (!str) return std::nullopt;
        try {
            return std::stoll(*str);
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Get flag value
     */
    bool flag(const std::string& name) const {
        auto it = flags_.find(name);
        return it != flags_.end() && it->second;
    }

    /**
     * @brief Get positional argument by index
     */
    std::optional<std::string> positional(size_t index) const {
        if (index < positional_values_.size()) {
            return positional_values_[index];
        }
        return std::nullopt;
    }

    /**
     * @brief Get all positional arguments
     */
    const std::vector<std::string>& positional_args() const {
        return positional_values_;
    }

    /**
     * @brief Get extra arguments (beyond defined positionals)
     */
    const std::vector<std::string>& extra_args() const {
        return extra_args_;
    }

    /**
     * @brief Get error message if parsing failed
     */
    const std::string& error() const {
        return error_;
    }

    /**
     * @brief Show help and usage
     */
    void show_help() const {
        std::cout << "Usage: " << program_name_;
        
        for (const auto& opt : options_) {
            if (opt.is_flag) {
                std::cout << " [--" << opt.long_name << "]";
            } else if (opt.required) {
                std::cout << " --" << opt.long_name << "=<value>";
            } else {
                std::cout << " [--" << opt.long_name << "=<value>]";
            }
        }
        
        for (size_t i = 0; i < positional_names_.size(); ++i) {
            if (positional_required_[i]) {
                std::cout << " <" << positional_names_[i] << ">";
            } else {
                std::cout << " [" << positional_names_[i] << "]";
            }
        }
        
        std::cout << "\n\n";
        
        if (!description_.empty()) {
            std::cout << description_ << "\n\n";
        }

        if (!options_.empty()) {
            std::cout << "Options:\n";
            for (const auto& opt : options_) {
                std::cout << "  ";
                if (opt.short_name) {
                    std::cout << "-" << opt.short_name << ", ";
                } else {
                    std::cout << "    ";
                }
                std::cout << "--" << std::left << std::setw(20) << opt.long_name;
                std::cout << opt.description;
                if (!opt.default_value.empty()) {
                    std::cout << " [default: " << opt.default_value << "]";
                }
                if (opt.required) {
                    std::cout << " (required)";
                }
                std::cout << "\n";
            }
        }

        if (!positional_names_.empty()) {
            std::cout << "\nArguments:\n";
            for (size_t i = 0; i < positional_names_.size(); ++i) {
                std::cout << "  " << std::left << std::setw(22) << positional_names_[i];
                std::cout << positional_descriptions_[i];
                if (positional_required_[i]) {
                    std::cout << " (required)";
                }
                std::cout << "\n";
            }
        }

        std::cout << "\n  -h, --help                Show this help message\n";
    }

private:
    Option* find_option(const std::string& name) {
        for (auto& opt : options_) {
            if (opt.long_name == name) return &opt;
        }
        return nullptr;
    }

    Option* find_option(char short_name) {
        for (auto& opt : options_) {
            if (opt.short_name == short_name) return &opt;
        }
        return nullptr;
    }

    std::string program_name_;
    std::string description_;
    std::vector<Option> options_;
    std::vector<std::string> positional_names_;
    std::vector<std::string> positional_descriptions_;
    std::vector<bool> positional_required_;
    
    std::map<std::string, std::string> values_;
    std::map<std::string, bool> flags_;
    std::vector<std::string> positional_values_;
    std::vector<std::string> extra_args_;
    std::string error_;
};

} // namespace cics::cli
