// =============================================================================
// CICS Emulation - Journal Control Implementation
// =============================================================================

#include <cics/journal/journal.hpp>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <ctime>

namespace cics {
namespace journal {

namespace fs = std::filesystem;

// =============================================================================
// Utility Functions
// =============================================================================

String journal_type_to_string(JournalType type) {
    switch (type) {
        case JournalType::USER: return "USER";
        case JournalType::SYSTEM: return "SYSTEM";
        case JournalType::AUTO: return "AUTO";
        default: return "UNKNOWN";
    }
}

String journal_status_to_string(JournalStatus status) {
    switch (status) {
        case JournalStatus::OPEN: return "OPEN";
        case JournalStatus::CLOSED: return "CLOSED";
        case JournalStatus::FULL: return "FULL";
        case JournalStatus::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// Journal Implementation
// =============================================================================

Journal::Journal(StringView name, UInt32 number, JournalType type)
    : name_(name)
    , number_(number)
    , type_(type)
{
}

Journal::~Journal() {
    if (is_open()) {
        close();
    }
}

Result<void> Journal::open(StringView filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ == JournalStatus::OPEN) {
        return make_error<void>(ErrorCode::INVREQ, "Journal already open");
    }
    
    filename_ = String(filename);
    
    // Create directory if needed
    fs::path dir = fs::path(filename_).parent_path();
    if (!dir.empty() && !fs::exists(dir)) {
        fs::create_directories(dir);
    }
    
    file_.open(filename_, std::ios::out | std::ios::app | std::ios::binary);
    if (!file_.is_open()) {
        status_ = JournalStatus::ERROR;
        return make_error<void>(ErrorCode::IOERR, "Failed to open journal file");
    }
    
    status_ = JournalStatus::OPEN;
    opened_ = std::chrono::system_clock::now();
    
    return make_success();
}

Result<void> Journal::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != JournalStatus::OPEN) {
        return make_success();  // Already closed
    }
    
    file_.flush();
    file_.close();
    status_ = JournalStatus::CLOSED;
    
    return make_success();
}

Result<UInt64> Journal::write(const JournalRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != JournalStatus::OPEN) {
        return make_error<UInt64>(ErrorCode::INVREQ, "Journal not open");
    }
    
    UInt64 seq = ++sequence_;
    
    // Write record header
    auto time_t_now = std::chrono::system_clock::to_time_t(record.timestamp);
    
    file_ << "===================================================================\n";
    file_ << "SEQUENCE: " << std::setw(12) << seq << "  ";
    file_ << "TIME: " << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") << "\n";
    file_ << "JOURNAL:  " << name_ << "  JTYPEID: " << record.jtypeid << "\n";
    if (!record.transaction_id.empty()) {
        file_ << "TRANSID:  " << record.transaction_id << "  TASKID: " << record.task_id << "\n";
    }
    if (!record.prefix.empty()) {
        file_ << "PREFIX:   " << record.prefix << "\n";
    }
    file_ << "LENGTH:   " << record.data.size() << " bytes\n";
    file_ << "-------------------------------------------------------------------\n";
    
    // Write data (as text if printable, otherwise hex dump)
    bool is_text = true;
    for (Byte b : record.data) {
        if (b < 0x20 || b > 0x7E) {
            if (b != 0x0A && b != 0x0D && b != 0x09) {
                is_text = false;
                break;
            }
        }
    }
    
    if (is_text && !record.data.empty()) {
        file_.write(reinterpret_cast<const char*>(record.data.data()), 
                    static_cast<std::streamsize>(record.data.size()));
        file_ << "\n";
    } else if (!record.data.empty()) {
        // Hex dump
        for (size_t i = 0; i < record.data.size(); i += 16) {
            file_ << std::hex << std::setfill('0') << std::setw(8) << i << "  ";
            for (size_t j = 0; j < 16 && i + j < record.data.size(); ++j) {
                file_ << std::setw(2) << static_cast<int>(record.data[i + j]) << " ";
            }
            file_ << " |";
            for (size_t j = 0; j < 16 && i + j < record.data.size(); ++j) {
                char c = static_cast<char>(record.data[i + j]);
                file_ << (c >= 0x20 && c <= 0x7E ? c : '.');
            }
            file_ << "|\n";
        }
        file_ << std::dec;
    }
    
    file_ << "\n";
    
    if (!file_) {
        status_ = JournalStatus::ERROR;
        return make_error<UInt64>(ErrorCode::IOERR, "Write failed");
    }
    
    ++records_written_;
    bytes_written_ += record.data.size();
    last_write_ = std::chrono::system_clock::now();
    
    return make_success(seq);
}

Result<UInt64> Journal::write(StringView jtypeid, const void* data, UInt32 length) {
    JournalRecord record;
    record.timestamp = std::chrono::system_clock::now();
    record.journal_name = name_;
    record.jtypeid = String(jtypeid);
    
    if (data && length > 0) {
        const Byte* bytes = static_cast<const Byte*>(data);
        record.data.assign(bytes, bytes + length);
    }
    record.length = length;
    
    return write(record);
}

Result<void> Journal::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != JournalStatus::OPEN) {
        return make_error<void>(ErrorCode::INVREQ, "Journal not open");
    }
    
    file_.flush();
    return make_success();
}

JournalInfo Journal::get_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    JournalInfo info;
    info.name = name_;
    info.number = number_;
    info.type = type_;
    info.status = status_;
    info.records_written = records_written_;
    info.bytes_written = bytes_written_;
    info.opened = opened_;
    info.last_write = last_write_;
    info.filename = filename_;
    return info;
}

// =============================================================================
// JournalManager Implementation
// =============================================================================

JournalManager& JournalManager::instance() {
    static JournalManager instance;
    return instance;
}

void JournalManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;
    
    journals_by_name_.clear();
    journals_by_number_.clear();
    
    // Create journal directory
    if (!fs::exists(journal_directory_)) {
        fs::create_directories(journal_directory_);
    }
    
    // Create system log journal
    auto syslog = std::make_unique<Journal>("DFHLOG", DFHLOG, JournalType::SYSTEM);
    syslog->open(generate_filename("DFHLOG"));
    journals_by_number_[DFHLOG] = syslog.get();
    journals_by_name_["DFHLOG"] = std::move(syslog);
    
    reset_stats();
    ++stats_.journals_opened;
    initialized_ = true;
}

void JournalManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [name, journal] : journals_by_name_) {
        if (journal->is_open()) {
            journal->close();
            ++stats_.journals_closed;
        }
    }
    
    journals_by_name_.clear();
    journals_by_number_.clear();
    initialized_ = false;
}

void JournalManager::set_journal_directory(StringView dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    journal_directory_ = String(dir);
    
    if (!fs::exists(journal_directory_)) {
        fs::create_directories(journal_directory_);
    }
}

String JournalManager::generate_filename(StringView name) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << journal_directory_ << "/" << name << "_"
        << std::put_time(std::localtime(&time_t_now), "%Y%m%d") << ".jrnl";
    return oss.str();
}

Result<Journal*> JournalManager::open_journal(StringView name, UInt32 number) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    String key(name);
    auto it = journals_by_name_.find(key);
    if (it != journals_by_name_.end()) {
        return make_success(it->second.get());
    }
    
    JournalType type = (number == DFHLOG || number == DFHSHUNT) 
                       ? JournalType::SYSTEM : JournalType::USER;
    
    auto journal = std::make_unique<Journal>(name, number, type);
    auto result = journal->open(generate_filename(name));
    if (result.is_error()) {
        return make_error<Journal*>(result.error().code, result.error().message);
    }
    
    Journal* ptr = journal.get();
    if (number > 0) {
        journals_by_number_[number] = ptr;
    }
    journals_by_name_[key] = std::move(journal);
    
    ++stats_.journals_opened;
    return make_success(ptr);
}

Result<Journal*> JournalManager::get_journal(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = journals_by_name_.find(String(name));
    if (it == journals_by_name_.end()) {
        return make_error<Journal*>(ErrorCode::NOTFND, "Journal not found: " + String(name));
    }
    
    return make_success(it->second.get());
}

Result<Journal*> JournalManager::get_journal(UInt32 number) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = journals_by_number_.find(number);
    if (it == journals_by_number_.end()) {
        return make_error<Journal*>(ErrorCode::NOTFND, 
            "Journal not found: " + std::to_string(number));
    }
    
    return make_success(it->second);
}

Result<void> JournalManager::close_journal(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = journals_by_name_.find(String(name));
    if (it == journals_by_name_.end()) {
        return make_error<void>(ErrorCode::NOTFND, "Journal not found");
    }
    
    auto result = it->second->close();
    ++stats_.journals_closed;
    
    // Remove from number map
    UInt32 num = it->second->number();
    journals_by_number_.erase(num);
    journals_by_name_.erase(it);
    
    return result;
}

Result<UInt64> JournalManager::write(StringView journal_name, StringView jtypeid,
                                      const void* data, UInt32 length) {
    auto journal_result = get_journal(journal_name);
    if (journal_result.is_error()) {
        // Auto-open journal
        journal_result = open_journal(journal_name);
        if (journal_result.is_error()) {
            return make_error<UInt64>(journal_result.error().code, journal_result.error().message);
        }
    }
    
    JournalRecord record;
    record.timestamp = std::chrono::system_clock::now();
    record.journal_name = String(journal_name);
    record.jtypeid = String(jtypeid);
    record.transaction_id = current_transid_;
    record.task_id = current_task_id_;
    
    if (data && length > 0) {
        const Byte* bytes = static_cast<const Byte*>(data);
        record.data.assign(bytes, bytes + length);
    }
    record.length = length;
    
    auto result = journal_result.value()->write(record);
    if (result.is_success()) {
        ++stats_.records_written;
        stats_.bytes_written += length;
    } else {
        ++stats_.errors;
    }
    return result;
}

Result<UInt64> JournalManager::write(UInt32 journal_number, StringView jtypeid,
                                      const void* data, UInt32 length) {
    auto journal_result = get_journal(journal_number);
    if (journal_result.is_error()) {
        return make_error<UInt64>(journal_result.error().code, journal_result.error().message);
    }
    
    auto result = journal_result.value()->write(jtypeid, data, length);
    if (result.is_success()) {
        ++stats_.records_written;
        stats_.bytes_written += length;
    } else {
        ++stats_.errors;
    }
    return result;
}

Result<UInt64> JournalManager::write(StringView journal_name, const JournalRecord& record) {
    auto journal_result = get_journal(journal_name);
    if (journal_result.is_error()) {
        journal_result = open_journal(journal_name);
        if (journal_result.is_error()) {
            return make_error<UInt64>(journal_result.error().code, journal_result.error().message);
        }
    }
    
    auto result = journal_result.value()->write(record);
    if (result.is_success()) {
        ++stats_.records_written;
        stats_.bytes_written += record.data.size();
    } else {
        ++stats_.errors;
    }
    return result;
}

Result<UInt64> JournalManager::log(StringView message) {
    return log("LOG", message);
}

Result<UInt64> JournalManager::log(StringView jtypeid, StringView message) {
    return write("DFHLOG", jtypeid, message.data(), static_cast<UInt32>(message.size()));
}

std::vector<String> JournalManager::list_journals() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<String> names;
    names.reserve(journals_by_name_.size());
    for (const auto& [name, journal] : journals_by_name_) {
        names.push_back(name);
    }
    return names;
}

std::vector<JournalInfo> JournalManager::list_journal_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<JournalInfo> infos;
    infos.reserve(journals_by_name_.size());
    for (const auto& [name, journal] : journals_by_name_) {
        infos.push_back(journal->get_info());
    }
    return infos;
}

JournalStats JournalManager::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void JournalManager::reset_stats() {
    stats_ = JournalStats{};
}

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

Result<UInt64> exec_cics_write_journalname(StringView journal_name,
                                            StringView jtypeid,
                                            const void* data, UInt32 length) {
    return JournalManager::instance().write(journal_name, jtypeid, data, length);
}

Result<UInt64> exec_cics_write_journalname(StringView journal_name,
                                            const ByteBuffer& data) {
    return JournalManager::instance().write(journal_name, "DATA",
                                             data.data(), static_cast<UInt32>(data.size()));
}

Result<UInt64> exec_cics_write_journalnum(UInt32 journal_number,
                                           StringView jtypeid,
                                           const void* data, UInt32 length) {
    return JournalManager::instance().write(journal_number, jtypeid, data, length);
}

Result<UInt64> exec_cics_log(StringView message) {
    return JournalManager::instance().log(message);
}

Result<UInt64> exec_cics_log(StringView jtypeid, StringView message) {
    return JournalManager::instance().log(jtypeid, message);
}

} // namespace journal
} // namespace cics
