// =============================================================================
// CICS Emulation - Logging Implementation
// =============================================================================

#include "cics/common/logging.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace cics::logging {

// LogEntry implementation
String LogEntry::format(bool colored, bool include_location) const {
    std::ostringstream oss;
    
    // Timestamp
    auto time_t = SystemClock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<Milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    // Level with optional color
    oss << " [";
    if (colored) oss << level_color(level);
    oss << std::setw(5) << to_string(level);
    if (colored) oss << "\033[0m";
    oss << "] ";
    
    // Logger name
    if (!logger_name.empty()) {
        oss << "[" << logger_name << "] ";
    }
    
    // Message
    oss << message;
    
    // Location
    if (include_location && location.file_name() != nullptr) {
        oss << " (" << location.file_name() << ":" << location.line() << ")";
    }
    
    return oss.str();
}

// ConsoleSink implementation
ConsoleSink::ConsoleSink(LogLevel level, bool colored) 
    : level_(level), colored_(colored) {}

void ConsoleSink::write(const LogEntry& entry) {
    if (entry.level < level_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto& out = entry.level >= LogLevel::ERR ? std::cerr : std::cout;
    out << entry.format(colored_, entry.level >= LogLevel::ERR) << "\n";
}

void ConsoleSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout.flush();
    std::cerr.flush();
}

// FileSink implementation
FileSink::FileSink(const Path& path, LogLevel level, Size max_size, UInt32 max_backups)
    : level_(level), file_path_(path), max_file_size_(max_size), max_backup_count_(max_backups) {
    std::filesystem::create_directories(path.parent_path());
    file_.open(path, std::ios::app);
    current_size_ = std::filesystem::exists(path) ? std::filesystem::file_size(path) : 0;
}

FileSink::~FileSink() { 
    if (file_.is_open()) file_.close(); 
}

void FileSink::write(const LogEntry& entry) {
    if (entry.level < level_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    rotate_if_needed();
    
    String line = entry.format(false, true) + "\n";
    file_ << line;
    current_size_ += line.size();
}

void FileSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) file_.flush();
}

void FileSink::rotate_if_needed() {
    if (current_size_ >= max_file_size_) rotate_files();
}

void FileSink::rotate_files() {
    file_.close();
    for (int i = static_cast<int>(max_backup_count_) - 1; i >= 0; --i) {
        Path old_path = file_path_.string() + (i > 0 ? "." + std::to_string(i) : "");
        Path new_path = file_path_.string() + "." + std::to_string(i + 1);
        if (std::filesystem::exists(old_path)) {
            if (i + 1 >= static_cast<int>(max_backup_count_)) std::filesystem::remove(old_path);
            else std::filesystem::rename(old_path, new_path);
        }
    }
    file_.open(file_path_, std::ios::out);
    current_size_ = 0;
}

// AsyncSink implementation
AsyncSink::AsyncSink(UniquePtr<LogSink> sink)
    : inner_sink_(std::move(sink)), level_(LogLevel::TRACE) {
    worker_ = std::thread(&AsyncSink::worker_loop, this);
}

AsyncSink::~AsyncSink() {
    running_ = false;
    cv_.notify_one();
    if (worker_.joinable()) worker_.join();
}

void AsyncSink::write(const LogEntry& entry) {
    if (entry.level < level_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(entry);
    cv_.notify_one();
}

void AsyncSink::flush() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() { return queue_.empty(); });
    inner_sink_->flush();
}

void AsyncSink::worker_loop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return !queue_.empty() || !running_; });
        if (!running_ && queue_.empty()) break;
        while (!queue_.empty()) {
            LogEntry entry = queue_.front();
            queue_.pop();
            lock.unlock();
            inner_sink_->write(entry);
            lock.lock();
        }
    }
}

// Logger implementation
Logger::Logger(String name) : name_(std::move(name)) {}

void Logger::add_sink(SharedPtr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::remove_all_sinks() {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

void Logger::log(LogLevel level, StringView message, std::source_location loc) {
    if (!should_log(level)) return;
    
    LogEntry entry;
    entry.level = level;
    entry.timestamp = SystemClock::now();
    entry.message = String(message);
    entry.logger_name = name_;
    entry.location = loc;
    
    // Get thread ID
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    entry.thread_id = oss.str();
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) {
        sink->write(entry);
    }
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) sink->flush();
}

// LogManager implementation
LogManager::LogManager() {
    root_logger_ = std::make_shared<Logger>("root");
    root_logger_->add_sink(std::make_shared<ConsoleSink>(LogLevel::INFO));
}

LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

SharedPtr<Logger> LogManager::get_logger(const String& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = loggers_.find(name);
    if (it != loggers_.end()) return it->second;
    
    auto logger = std::make_shared<Logger>(name);
    // Copy sinks from root
    for (auto& sink : root_logger_->sinks_) {
        logger->add_sink(sink);
    }
    logger->set_level(root_logger_->level());
    loggers_[name] = logger;
    return logger;
}

SharedPtr<Logger> LogManager::get_root_logger() { 
    return root_logger_; 
}

void LogManager::set_global_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    root_logger_->set_level(level);
    for (auto& [_, logger] : loggers_) logger->set_level(level);
}

void LogManager::add_global_sink(SharedPtr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    root_logger_->add_sink(sink);
    for (auto& [_, logger] : loggers_) logger->add_sink(sink);
}

void LogManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, logger] : loggers_) logger->flush();
    root_logger_->flush();
}

void LogManager::configure_default(LogLevel console_level, Optional<Path> log_file, LogLevel file_level) {
    root_logger_->remove_all_sinks();
    root_logger_->add_sink(std::make_shared<ConsoleSink>(console_level));
    if (log_file) {
        root_logger_->add_sink(std::make_shared<FileSink>(*log_file, file_level));
    }
}

// ScopedTimer implementation
ScopedTimer::ScopedTimer(SharedPtr<Logger> logger, String operation, LogLevel level, std::source_location loc)
    : logger_(std::move(logger)), operation_name_(std::move(operation)), 
      start_time_(Clock::now()), level_(level), location_(loc) {}

ScopedTimer::~ScopedTimer() {
    auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start_time_);
    logger_->log(level_, std::format("{} completed in {}us", operation_name_, elapsed.count()), location_);
}

} // namespace cics::logging
