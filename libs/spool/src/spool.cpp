// =============================================================================
// CICS Emulation - Spool Control Implementation
// =============================================================================

#include <cics/spool/spool.hpp>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace cics {
namespace spool {

namespace fs = std::filesystem;

// =============================================================================
// Utility Functions
// =============================================================================

String spool_type_to_string(SpoolType type) {
    switch (type) {
        case SpoolType::INPUT: return "INPUT";
        case SpoolType::OUTPUT: return "OUTPUT";
        default: return "UNKNOWN";
    }
}

char spool_class_to_char(SpoolClass cls) {
    return static_cast<char>(cls);
}

SpoolClass char_to_spool_class(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<SpoolClass>(c);
    }
    return SpoolClass::STAR;
}

String spool_disposition_to_string(SpoolDisposition disp) {
    switch (disp) {
        case SpoolDisposition::KEEP: return "KEEP";
        case SpoolDisposition::DELETE: return "DELETE";
        case SpoolDisposition::HOLD: return "HOLD";
        case SpoolDisposition::RELEASE: return "RELEASE";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// SpoolFile Implementation
// =============================================================================

SpoolFile::SpoolFile(StringView token, const SpoolAttributes& attrs)
    : token_(token)
    , attrs_(attrs)
{
}

SpoolFile::~SpoolFile() {
    if (is_open_) {
        close();
    }
}

Result<void> SpoolFile::open() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (is_open_) {
        return make_error<void>(ErrorCode::INVREQ, "Spool file already open");
    }
    
    // Generate file path
    auto& mgr = SpoolManager::instance();
    std::ostringstream path;
    path << mgr.spool_directory() << "/" << token_ << "_" << attrs_.name << ".spool";
    file_path_ = path.str();
    
    // Create directory if needed
    fs::path dir(mgr.spool_directory());
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
    
    // Open file
    if (attrs_.type == SpoolType::OUTPUT) {
        file_.open(file_path_, std::ios::out | std::ios::binary);
    } else {
        file_.open(file_path_, std::ios::in | std::ios::binary);
    }
    
    if (!file_.is_open()) {
        return make_error<void>(ErrorCode::IOERR,
            "Failed to open spool file: " + file_path_);
    }
    
    is_open_ = true;
    record_count_ = 0;
    byte_count_ = 0;
    current_line_ = 0;
    current_page_ = 1;
    
    return make_success();
}

Result<void> SpoolFile::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return make_error<void>(ErrorCode::INVREQ, "Spool file not open");
    }
    
    file_.close();
    is_open_ = false;
    
    // Handle disposition
    if (attrs_.disposition == SpoolDisposition::DELETE) {
        fs::remove(file_path_);
    }
    
    return make_success();
}

Result<void> SpoolFile::write(StringView data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return make_error<void>(ErrorCode::INVREQ, "Spool file not open");
    }
    
    if (attrs_.type != SpoolType::OUTPUT) {
        return make_error<void>(ErrorCode::INVREQ, "Cannot write to input spool");
    }
    
    file_.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!file_) {
        return make_error<void>(ErrorCode::IOERR, "Write failed");
    }
    
    byte_count_ += data.size();
    ++record_count_;
    
    return make_success();
}

Result<void> SpoolFile::write(const ByteBuffer& data) {
    return write(StringView(reinterpret_cast<const char*>(data.data()), data.size()));
}

Result<void> SpoolFile::write_line(StringView line) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return make_error<void>(ErrorCode::INVREQ, "Spool file not open");
    }
    
    if (attrs_.type != SpoolType::OUTPUT) {
        return make_error<void>(ErrorCode::INVREQ, "Cannot write to input spool");
    }
    
    // Check for page break
    if (attrs_.page_numbers && current_line_ >= attrs_.lines_per_page) {
        file_ << "\f";  // Form feed
        ++current_page_;
        current_line_ = 0;
    }
    
    // Write line number if enabled
    if (attrs_.line_numbers) {
        file_ << std::setw(6) << record_count_ + 1 << " ";
    }
    
    // Write the line
    file_ << line << "\n";
    
    if (!file_) {
        return make_error<void>(ErrorCode::IOERR, "Write failed");
    }
    
    byte_count_ += line.size() + 1;
    ++record_count_;
    ++current_line_;
    
    return make_success();
}

Result<void> SpoolFile::new_page() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return make_error<void>(ErrorCode::INVREQ, "Spool file not open");
    }
    
    if (attrs_.type != SpoolType::OUTPUT) {
        return make_error<void>(ErrorCode::INVREQ, "Cannot write to input spool");
    }
    
    file_ << "\f";  // Form feed
    ++current_page_;
    current_line_ = 0;
    
    return make_success();
}

Result<ByteBuffer> SpoolFile::read() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return make_error<ByteBuffer>(ErrorCode::INVREQ, "Spool file not open");
    }
    
    if (attrs_.type != SpoolType::INPUT) {
        return make_error<ByteBuffer>(ErrorCode::INVREQ, "Cannot read from output spool");
    }
    
    ByteBuffer buffer(attrs_.record_length);
    file_.read(reinterpret_cast<char*>(buffer.data()), attrs_.record_length);
    
    auto bytes_read = file_.gcount();
    if (bytes_read == 0) {
        return make_error<ByteBuffer>(ErrorCode::ENDFILE, "End of file");
    }
    
    buffer.resize(static_cast<size_t>(bytes_read));
    byte_count_ += static_cast<UInt64>(bytes_read);
    ++record_count_;
    
    return make_success(buffer);
}

Result<String> SpoolFile::read_line() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return make_error<String>(ErrorCode::INVREQ, "Spool file not open");
    }
    
    if (attrs_.type != SpoolType::INPUT) {
        return make_error<String>(ErrorCode::INVREQ, "Cannot read from output spool");
    }
    
    String line;
    if (!std::getline(file_, line)) {
        return make_error<String>(ErrorCode::ENDFILE, "End of file");
    }
    
    byte_count_ += line.size();
    ++record_count_;
    ++current_line_;
    
    return make_success(line);
}

bool SpoolFile::eof() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return file_.eof();
}

Result<void> SpoolFile::rewind() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return make_error<void>(ErrorCode::INVREQ, "Spool file not open");
    }
    
    file_.clear();
    file_.seekg(0, std::ios::beg);
    current_line_ = 0;
    
    return make_success();
}

Result<void> SpoolFile::skip(UInt32 records) {
    for (UInt32 i = 0; i < records; ++i) {
        auto result = read();
        if (result.is_error()) {
            return make_error<void>(result.error().code, result.error().message);
        }
    }
    return make_success();
}

SpoolInfo SpoolFile::get_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SpoolInfo info;
    info.token = token_;
    info.name = attrs_.name;
    info.type = attrs_.type;
    info.spool_class = attrs_.spool_class;
    info.record_count = record_count_;
    info.byte_count = byte_count_;
    info.page_count = current_page_;
    info.created = std::chrono::system_clock::now();  // Simplified
    info.modified = info.created;
    info.is_open = is_open_;
    return info;
}

// =============================================================================
// SpoolManager Implementation
// =============================================================================

SpoolManager& SpoolManager::instance() {
    static SpoolManager instance;
    return instance;
}

void SpoolManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;
    
    files_.clear();
    token_counter_ = 0;
    
    // Create spool directory
    if (!fs::exists(spool_directory_)) {
        fs::create_directories(spool_directory_);
    }
    
    reset_stats();
    initialized_ = true;
}

void SpoolManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Close all open files
    for (auto& [token, file] : files_) {
        if (file->is_open()) {
            file->close();
        }
    }
    
    files_.clear();
    initialized_ = false;
}

void SpoolManager::set_spool_directory(StringView dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    spool_directory_ = String(dir);
    
    if (!fs::exists(spool_directory_)) {
        fs::create_directories(spool_directory_);
    }
}

String SpoolManager::generate_token() {
    UInt64 counter = ++token_counter_;
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << "SP" << std::put_time(std::localtime(&time_t_now), "%H%M%S")
        << std::setfill('0') << std::setw(6) << counter;
    return oss.str();
}

Result<String> SpoolManager::open(const SpoolAttributes& attrs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return make_error<String>(ErrorCode::NOT_INITIALIZED,
            "SpoolManager not initialized");
    }
    
    String token = generate_token();
    auto file = std::make_unique<SpoolFile>(token, attrs);
    
    auto result = file->open();
    if (result.is_error()) {
        return make_error<String>(result.error().code, result.error().message);
    }
    
    files_[token] = std::move(file);
    ++stats_.files_opened;
    
    return make_success(token);
}

Result<void> SpoolManager::close(StringView token) {
    return close(token, SpoolDisposition::KEEP);
}

Result<void> SpoolManager::close(StringView token, SpoolDisposition disp) {
    (void)disp;  // Disposition handled by SpoolFile
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = files_.find(String(token));
    if (it == files_.end()) {
        return make_error<void>(ErrorCode::INVREQ, "Invalid spool token");
    }
    
    auto result = it->second->close();
    files_.erase(it);
    
    ++stats_.files_closed;
    return result;
}

SpoolFile* SpoolManager::get_file(StringView token) {
    auto it = files_.find(String(token));
    if (it == files_.end()) {
        return nullptr;
    }
    return it->second.get();
}

Result<void> SpoolManager::write(StringView token, StringView data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SpoolFile* file = get_file(token);
    if (!file) {
        return make_error<void>(ErrorCode::INVREQ, "Invalid spool token");
    }
    
    auto result = file->write(data);
    if (result.is_success()) {
        ++stats_.records_written;
        stats_.bytes_written += data.size();
    }
    return result;
}

Result<void> SpoolManager::write(StringView token, const ByteBuffer& data) {
    return write(token, StringView(reinterpret_cast<const char*>(data.data()), data.size()));
}

Result<void> SpoolManager::write_line(StringView token, StringView line) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SpoolFile* file = get_file(token);
    if (!file) {
        return make_error<void>(ErrorCode::INVREQ, "Invalid spool token");
    }
    
    auto result = file->write_line(line);
    if (result.is_success()) {
        ++stats_.records_written;
        stats_.bytes_written += line.size();
    }
    return result;
}

Result<ByteBuffer> SpoolManager::read(StringView token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SpoolFile* file = get_file(token);
    if (!file) {
        return make_error<ByteBuffer>(ErrorCode::INVREQ, "Invalid spool token");
    }
    
    auto result = file->read();
    if (result.is_success()) {
        ++stats_.records_read;
        stats_.bytes_read += result.value().size();
    }
    return result;
}

Result<String> SpoolManager::read_line(StringView token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SpoolFile* file = get_file(token);
    if (!file) {
        return make_error<String>(ErrorCode::INVREQ, "Invalid spool token");
    }
    
    auto result = file->read_line();
    if (result.is_success()) {
        ++stats_.records_read;
        stats_.bytes_read += result.value().size();
    }
    return result;
}

Result<void> SpoolManager::new_page(StringView token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SpoolFile* file = get_file(token);
    if (!file) {
        return make_error<void>(ErrorCode::INVREQ, "Invalid spool token");
    }
    
    auto result = file->new_page();
    if (result.is_success()) {
        ++stats_.pages_output;
    }
    return result;
}

Result<void> SpoolManager::rewind(StringView token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SpoolFile* file = get_file(token);
    if (!file) {
        return make_error<void>(ErrorCode::INVREQ, "Invalid spool token");
    }
    
    return file->rewind();
}

Result<SpoolInfo> SpoolManager::get_info(StringView token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SpoolFile* file = get_file(token);
    if (!file) {
        return make_error<SpoolInfo>(ErrorCode::INVREQ, "Invalid spool token");
    }
    
    return make_success(file->get_info());
}

std::vector<SpoolInfo> SpoolManager::list_spools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SpoolInfo> infos;
    infos.reserve(files_.size());
    for (const auto& [token, file] : files_) {
        infos.push_back(file->get_info());
    }
    return infos;
}

SpoolStats SpoolManager::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void SpoolManager::reset_stats() {
    stats_.files_opened = 0;
    stats_.files_closed = 0;
    stats_.records_written = 0;
    stats_.records_read = 0;
    stats_.bytes_written = 0;
    stats_.bytes_read = 0;
    stats_.pages_output = 0;
}

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

Result<String> exec_cics_spoolopen_output(StringView name) {
    SpoolAttributes attrs;
    attrs.name = String(name);
    attrs.type = SpoolType::OUTPUT;
    return SpoolManager::instance().open(attrs);
}

Result<String> exec_cics_spoolopen_output(StringView name, SpoolClass spool_class) {
    SpoolAttributes attrs;
    attrs.name = String(name);
    attrs.type = SpoolType::OUTPUT;
    attrs.spool_class = spool_class;
    return SpoolManager::instance().open(attrs);
}

Result<String> exec_cics_spoolopen_output(const SpoolAttributes& attrs) {
    return SpoolManager::instance().open(attrs);
}

Result<String> exec_cics_spoolopen_input(StringView name) {
    SpoolAttributes attrs;
    attrs.name = String(name);
    attrs.type = SpoolType::INPUT;
    return SpoolManager::instance().open(attrs);
}

Result<void> exec_cics_spoolwrite(StringView token, StringView data) {
    return SpoolManager::instance().write(token, data);
}

Result<void> exec_cics_spoolwrite(StringView token, const ByteBuffer& data) {
    return SpoolManager::instance().write(token, data);
}

Result<void> exec_cics_spoolwrite_line(StringView token, StringView line) {
    return SpoolManager::instance().write_line(token, line);
}

Result<ByteBuffer> exec_cics_spoolread(StringView token) {
    return SpoolManager::instance().read(token);
}

Result<String> exec_cics_spoolread_line(StringView token) {
    return SpoolManager::instance().read_line(token);
}

Result<void> exec_cics_spoolclose(StringView token) {
    return SpoolManager::instance().close(token);
}

Result<void> exec_cics_spoolclose(StringView token, SpoolDisposition disp) {
    return SpoolManager::instance().close(token, disp);
}

} // namespace spool
} // namespace cics
