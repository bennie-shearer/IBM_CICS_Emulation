// =============================================================================
// CICS Emulation - Logging System
// =============================================================================
#pragma once

#include "cics/common/types.hpp"
#include <fstream>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <source_location>

namespace cics::logging {

using namespace cics;

// Log levels - avoid DEBUG name due to Windows macro conflict
enum class LogLevel : UInt8 {
    TRACE = 0,
    DBG = 1,      // Renamed from DEBUG to avoid -DDEBUG macro conflict
    INFO = 2,
    WARN = 3,
    ERR = 4,      // Renamed from ERROR to avoid Windows ERROR macro
    FATAL = 5,
    OFF = 6
};

[[nodiscard]] constexpr StringView to_string(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DBG:   return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERR:   return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::OFF:   return "OFF";
    }
    return "UNKNOWN";
}

[[nodiscard]] constexpr StringView level_color(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "\033[90m";   // Gray
        case LogLevel::DBG:   return "\033[36m";   // Cyan
        case LogLevel::INFO:  return "\033[32m";   // Green
        case LogLevel::WARN:  return "\033[33m";   // Yellow
        case LogLevel::ERR:   return "\033[31m";   // Red
        case LogLevel::FATAL: return "\033[35m";   // Magenta
        default: return "\033[0m";
    }
}

// Log entry
struct LogEntry {
    LogLevel level = LogLevel::INFO;
    SystemTimePoint timestamp = SystemClock::now();
    String message;
    String logger_name;
    String thread_id;
    std::source_location location;
    
    [[nodiscard]] String format(bool colored = false, bool include_location = false) const;
};

// Abstract sink interface
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogEntry& entry) = 0;
    virtual void flush() = 0;
    [[nodiscard]] virtual LogLevel get_level() const = 0;
    virtual void set_level(LogLevel level) = 0;
};

// Console sink
class ConsoleSink : public LogSink {
private:
    LogLevel level_;
    bool colored_;
    mutable std::mutex mutex_;
    
public:
    explicit ConsoleSink(LogLevel level = LogLevel::INFO, bool colored = true);
    
    void write(const LogEntry& entry) override;
    void flush() override;
    [[nodiscard]] LogLevel get_level() const override { return level_; }
    void set_level(LogLevel level) override { level_ = level; }
};

// File sink with rotation
class FileSink : public LogSink {
private:
    LogLevel level_;
    Path file_path_;
    std::ofstream file_;
    Size max_file_size_;
    UInt32 max_backup_count_;
    Size current_size_ = 0;
    mutable std::mutex mutex_;
    
    void rotate_if_needed();
    void rotate_files();
    
public:
    FileSink(const Path& path, LogLevel level = LogLevel::DBG,
             Size max_size = 10 * 1024 * 1024, UInt32 max_backups = 5);
    ~FileSink() override;
    
    void write(const LogEntry& entry) override;
    void flush() override;
    [[nodiscard]] LogLevel get_level() const override { return level_; }
    void set_level(LogLevel level) override { level_ = level; }
};

// Async sink wrapper
class AsyncSink : public LogSink {
private:
    UniquePtr<LogSink> inner_sink_;
    LogLevel level_;
    std::queue<LogEntry> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    bool running_ = true;
    
    void worker_loop();
    
public:
    explicit AsyncSink(UniquePtr<LogSink> sink);
    ~AsyncSink() override;
    
    void write(const LogEntry& entry) override;
    void flush() override;
    [[nodiscard]] LogLevel get_level() const override { return level_; }
    void set_level(LogLevel level) override { level_ = level; }
};

// Logger class
class Logger {
    friend class LogManager;  // Allow LogManager to access sinks_
private:
    String name_;
    LogLevel level_ = LogLevel::INFO;
    std::vector<SharedPtr<LogSink>> sinks_;
    mutable std::mutex mutex_;
    
public:
    Logger() = default;
    explicit Logger(String name);
    
    void add_sink(SharedPtr<LogSink> sink);
    void remove_all_sinks();
    
    void log(LogLevel level, StringView message,
             std::source_location loc = std::source_location::current());
    
    template<typename... Args>
    void trace(std::format_string<Args...> fmt, Args&&... args) {
        if (should_log(LogLevel::TRACE)) {
            log(LogLevel::TRACE, std::format(fmt, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args) {
        if (should_log(LogLevel::DBG)) {
            log(LogLevel::DBG, std::format(fmt, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
        if (should_log(LogLevel::INFO)) {
            log(LogLevel::INFO, std::format(fmt, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args) {
        if (should_log(LogLevel::WARN)) {
            log(LogLevel::WARN, std::format(fmt, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        if (should_log(LogLevel::ERR)) {
            log(LogLevel::ERR, std::format(fmt, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void fatal(std::format_string<Args...> fmt, Args&&... args) {
        if (should_log(LogLevel::FATAL)) {
            log(LogLevel::FATAL, std::format(fmt, std::forward<Args>(args)...));
        }
    }
    
    void flush();
    
    [[nodiscard]] const String& name() const { return name_; }
    [[nodiscard]] LogLevel level() const { return level_; }
    void set_level(LogLevel level) { level_ = level; }
    
    [[nodiscard]] bool should_log(LogLevel level) const {
        return level >= level_ && level != LogLevel::OFF;
    }
};

// Log manager singleton
class LogManager {
private:
    SharedPtr<Logger> root_logger_;
    std::unordered_map<String, SharedPtr<Logger>> loggers_;
    mutable std::mutex mutex_;
    
    LogManager();
    
public:
    static LogManager& instance();
    
    [[nodiscard]] SharedPtr<Logger> get_logger(const String& name);
    [[nodiscard]] SharedPtr<Logger> get_root_logger();
    
    void set_global_level(LogLevel level);
    void add_global_sink(SharedPtr<LogSink> sink);
    void shutdown();
    
    void configure_default(LogLevel console_level = LogLevel::INFO,
                          Optional<Path> log_file = std::nullopt,
                          LogLevel file_level = LogLevel::DBG);
};

// Scoped timer for performance logging
class ScopedTimer {
private:
    SharedPtr<Logger> logger_;
    String operation_name_;
    TimePoint start_time_;
    LogLevel level_;
    std::source_location location_;
    
public:
    ScopedTimer(SharedPtr<Logger> logger, String operation,
                LogLevel level = LogLevel::DBG,
                std::source_location loc = std::source_location::current());
    ~ScopedTimer();
    
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
};

} // namespace cics::logging
