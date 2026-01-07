#include "cics/cics/cics_types.hpp"

namespace cics::cics {

EIB::EIB() { reset(); }

void EIB::reset() {
    eibtime = 0; eibdate = 0;
    eibtrnid = FixedString<4>(); eibtaskn = FixedString<4>(); eibtrmid = FixedString<4>();
    eibfn = FixedString<8>(); eibresp = CicsResponse::NORMAL; eibresp2 = CicsResponse2::NORMAL;
    eibcalen = 0; eibds = FixedString<8>(); eibreqid = FixedString<8>();
    eibrsrce = FixedString<8>(); eibcposn = 0; eibaid = 0;
    eibatt = eibeoc = eibfmh = eibcompl = eibsig = eibconf = false;
    eiberr = eibfree = eibrecv = eibsend = eibsync = eibnodat = false;
}

void EIB::set_time_date() {
    auto now = SystemClock::now();
    auto time_t = SystemClock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif
    eibtime = (tm.tm_hour * 10000 + tm.tm_min * 100 + tm.tm_sec) * 10;
    int year = tm.tm_year + 1900;
    int century = (year >= 2000) ? 1 : 0;
    eibdate = century * 1000000 + (year % 100) * 1000 + tm.tm_yday + 1;
}

String EIB::response_name() const {
    return String(::cics::cics::response_name(eibresp));
}

void Commarea::resize(Size new_size) {
    if (new_size > MAX_COMMAREA_LENGTH) new_size = MAX_COMMAREA_LENGTH;
    data_.resize(new_size);
}

void Commarea::set_data(ConstByteSpan data) {
    Size len = std::min(data.size(), max_length_);
    data_.assign(data.begin(), data.begin() + len);
}

void Commarea::set_string(Size offset, StringView str, Size field_length) {
    if (offset + field_length > data_.size()) data_.resize(offset + field_length);
    Size copy_len = std::min(str.size(), field_length);
    std::copy_n(str.begin(), copy_len, reinterpret_cast<char*>(data_.data() + offset));
    std::fill_n(reinterpret_cast<char*>(data_.data() + offset + copy_len), field_length - copy_len, ' ');
}

String Commarea::get_string(Size offset, Size length) const {
    if (offset >= data_.size()) return "";
    length = std::min(length, data_.size() - offset);
    return String(reinterpret_cast<const char*>(data_.data() + offset), length);
}

TransactionDefinition::TransactionDefinition(StringView txn_id, StringView pgm_name)
    : transaction_id(txn_id), program_name(pgm_name) {}

ProgramDefinition::ProgramDefinition(StringView name) : program_name(name) {}

FileDefinition::FileDefinition(StringView name, StringView dsn, FileType ft)
    : file_name(name), dataset_name(dsn), type(ft) {
    access_mode = static_cast<UInt8>(FileAccess::READ) | static_cast<UInt8>(FileAccess::WRITE);
}

CicsTask::CicsTask(UInt32 task_num, StringView txn_id, StringView term_id)
    : task_number_(task_num), transaction_id_(txn_id), terminal_id_(term_id)
    , status_(TransactionStatus::ACTIVE), start_time_(SystemClock::now()) {
    eib_.eibtrnid = FixedString<4>(txn_id);
    eib_.eibtrmid = FixedString<4>(term_id);
    eib_.set_time_date();
}

void CicsStatistics::record_transaction(Duration response_time, bool success, bool abend) {
    total_transactions++;
    if (success) successful_transactions++;
    else failed_transactions++;
    if (abend) abended_transactions++;
    
    auto ms = std::chrono::duration_cast<Milliseconds>(response_time).count();
    total_response_time_ms += ms;
}

void CicsStatistics::update_active_tasks(Int32 delta) {
    active_tasks += delta;
    UInt64 current = active_tasks.get();
    UInt64 peak = peak_tasks.get();
    // Update peak if current exceeds it
    while (current > peak) {
        peak = peak_tasks.get();
        if (current > peak) {
            // Try to update peak (simplified - in production use atomic CAS)
            break;
        }
    }
}

double CicsStatistics::average_response_ms() const {
    auto total = total_transactions.get();
    return total > 0 ? static_cast<double>(total_response_time_ms) / static_cast<double>(total) : 0.0;
}

double CicsStatistics::transactions_per_second() const {
    auto elapsed = std::chrono::duration_cast<Seconds>(SystemClock::now() - start_time).count();
    return elapsed > 0 ? static_cast<double>(total_transactions) / static_cast<double>(elapsed) : 0.0;
}

double CicsStatistics::success_rate() const {
    auto total = total_transactions.get();
    return total > 0 ? static_cast<double>(successful_transactions) * 100.0 / static_cast<double>(total) : 100.0;
}

String CicsStatistics::to_string() const {
    return std::format("Transactions: {} ({:.1f}% success), Avg: {:.1f}ms, TPS: {:.1f}",
        total_transactions.get(), success_rate(), average_response_ms(), transactions_per_second());
}

String CicsStatistics::to_json() const {
    return std::format(R"({{"transactions":{},"success_rate":{:.1f},"avg_ms":{:.1f},"tps":{:.1f}}})",
        total_transactions.get(), success_rate(), average_response_ms(), transactions_per_second());
}

StringView response_name(CicsResponse resp) {
    switch (resp) {
        case CicsResponse::NORMAL: return "NORMAL";
        case CicsResponse::ERROR: return "ERROR";
        case CicsResponse::NOTFND: return "NOTFND";
        case CicsResponse::DUPREC: return "DUPREC";
        case CicsResponse::INVREQ: return "INVREQ";
        case CicsResponse::IOERR: return "IOERR";
        case CicsResponse::NOSPACE: return "NOSPACE";
        case CicsResponse::NOTOPEN: return "NOTOPEN";
        case CicsResponse::ENDFILE: return "ENDFILE";
        case CicsResponse::LENGERR: return "LENGERR";
        case CicsResponse::PGMIDERR: return "PGMIDERR";
        case CicsResponse::TRANSIDERR: return "TRANSIDERR";
        case CicsResponse::NOTAUTH: return "NOTAUTH";
        case CicsResponse::DISABLED: return "DISABLED";
        default: return "UNKNOWN";
    }
}

StringView command_name(CicsCommand cmd) {
    switch (cmd) {
        case CicsCommand::READ: return "READ";
        case CicsCommand::WRITE: return "WRITE";
        case CicsCommand::REWRITE: return "REWRITE";
        case CicsCommand::DELETE_: return "DELETE";
        case CicsCommand::STARTBR: return "STARTBR";
        case CicsCommand::READNEXT: return "READNEXT";
        case CicsCommand::LINK: return "LINK";
        case CicsCommand::XCTL: return "XCTL";
        case CicsCommand::RETURN: return "RETURN";
        case CicsCommand::SEND: return "SEND";
        case CicsCommand::RECEIVE: return "RECEIVE";
        default: return "UNKNOWN";
    }
}

StringView status_name(TransactionStatus status) {
    switch (status) {
        case TransactionStatus::ACTIVE: return "ACTIVE";
        case TransactionStatus::SUSPENDED: return "SUSPENDED";
        case TransactionStatus::WAITING: return "WAITING";
        case TransactionStatus::RUNNING: return "RUNNING";
        case TransactionStatus::COMPLETED: return "COMPLETED";
        case TransactionStatus::ABENDED: return "ABENDED";
    }
    return "UNKNOWN";
}

} // namespace cics::cics
