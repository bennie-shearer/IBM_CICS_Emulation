// =============================================================================
// CICS Emulation - Transient Data Queue Implementation
// Version: 3.4.6
// =============================================================================

#include "cics/tdq/tdq_types.hpp"
#include <algorithm>
#include <format>
#include <sstream>

namespace cics::tdq {

// =============================================================================
// TDQRecord Implementation
// =============================================================================

String TDQRecord::to_string() const {
    return String(reinterpret_cast<const char*>(data_.data()), data_.size());
}

// =============================================================================
// TDQDefinition Implementation
// =============================================================================

Result<void> TDQDefinition::validate() const {
    if (dest_id.empty()) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Destination ID cannot be empty");
    }
    
    switch (type) {
        case TDQType::EXTRAPARTITION:
            if (file_path.empty()) {
                return make_error<void>(ErrorCode::INVALID_ARGUMENT, 
                    "File path required for extrapartition destination");
            }
            break;
        case TDQType::INDIRECT:
            if (indirect_dest.empty()) {
                return make_error<void>(ErrorCode::INVALID_ARGUMENT,
                    "Indirect destination required");
            }
            break;
        case TDQType::REMOTE:
            if (remote_sysid.empty()) {
                return make_error<void>(ErrorCode::INVALID_ARGUMENT,
                    "Remote system ID required");
            }
            break;
        default:
            break;
    }
    
    return {};
}

// =============================================================================
// TDQStatistics Implementation
// =============================================================================

TDQStatistics::TDQStatistics()
    : created(SystemClock::now())
    , last_write(created)
    , last_read(created) {}

void TDQStatistics::record_write(Size bytes) {
    ++total_records_written;
    ++current_depth;
    total_bytes_written += bytes;
    last_write = SystemClock::now();
}

void TDQStatistics::record_read(Size bytes) {
    ++total_records_read;
    --current_depth;
    total_bytes_read += bytes;
    last_read = SystemClock::now();
}

void TDQStatistics::record_trigger() {
    ++trigger_count;
}

void TDQStatistics::update_peak_depth(UInt64 depth) {
    if (depth > peak_depth) {
        peak_depth = depth;
    }
}

String TDQStatistics::to_string() const {
    std::ostringstream oss;
    oss << "TDQ Statistics:\n"
        << "  Current Depth: " << current_depth.get() << " (peak: " << peak_depth << ")\n"
        << "  Records Written: " << total_records_written.get() << "\n"
        << "  Records Read: " << total_records_read.get() << "\n"
        << "  Bytes Written: " << total_bytes_written.get() << "\n"
        << "  Bytes Read: " << total_bytes_read.get() << "\n"
        << "  Triggers: " << trigger_count.get();
    return oss.str();
}

String TDQStatistics::to_json() const {
    return std::format(R"({{"current_depth":{},"peak_depth":{},"records_written":{},"records_read":{},"bytes_written":{},"bytes_read":{},"triggers":{}}})",
        current_depth.get(), peak_depth,
        total_records_written.get(), total_records_read.get(),
        total_bytes_written.get(), total_bytes_read.get(),
        trigger_count.get());
}

// =============================================================================
// IntrapartitionQueue Implementation
// =============================================================================

IntrapartitionQueue::IntrapartitionQueue(TDQDefinition def)
    : definition_(std::move(def)) {
    statistics_.created = SystemClock::now();
}

void IntrapartitionQueue::check_trigger() {
    if (!definition_.trigger.has_value()) return;
    
    const auto& trigger = definition_.trigger.value();
    if (!trigger.enabled) return;
    
    UInt64 depth = records_.size();
    if (depth >= trigger.trigger_level) {
        statistics_.record_trigger();
        if (trigger.callback) {
            trigger.callback(trigger.transaction_id, definition_.dest_id.trimmed());
        }
    }
}

Result<void> IntrapartitionQueue::write(ConstByteSpan data) {
    std::unique_lock lock(mutex_);
    
    if (!enabled_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "Queue is disabled");
    }
    
    if (data.size() > MAX_RECORD_LENGTH) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT,
            std::format("Record length {} exceeds maximum {}", data.size(), MAX_RECORD_LENGTH));
    }
    
    if (definition_.max_records > 0 && records_.size() >= definition_.max_records) {
        return make_error<void>(ErrorCode::RESOURCE_EXHAUSTED,
            std::format("Queue full: {} records", definition_.max_records));
    }
    
    ++sequence_counter_;
    records_.emplace(data, sequence_counter_);
    
    statistics_.record_write(data.size());
    statistics_.update_peak_depth(records_.size());
    
    check_trigger();
    
    return {};
}

Result<void> IntrapartitionQueue::write(StringView str) {
    return write(ConstByteSpan(reinterpret_cast<const Byte*>(str.data()), str.size()));
}

Result<TDQRecord> IntrapartitionQueue::read() {
    std::unique_lock lock(mutex_);
    
    if (!enabled_) {
        return make_error<TDQRecord>(ErrorCode::INVALID_STATE, "Queue is disabled");
    }
    
    if (records_.empty()) {
        return make_error<TDQRecord>(ErrorCode::VSAM_END_OF_FILE, "Queue is empty");
    }
    
    TDQRecord record = std::move(records_.front());
    
    if (definition_.disposition == TDQDisposition::DELETE) {
        records_.pop();
    }
    
    statistics_.record_read(record.length());
    
    return record;
}

Result<TDQRecord> IntrapartitionQueue::peek() const {
    std::shared_lock lock(mutex_);
    
    if (records_.empty()) {
        return make_error<TDQRecord>(ErrorCode::VSAM_END_OF_FILE, "Queue is empty");
    }
    
    return records_.front();
}

Size IntrapartitionQueue::depth() const {
    std::shared_lock lock(mutex_);
    return records_.size();
}

bool IntrapartitionQueue::empty() const {
    std::shared_lock lock(mutex_);
    return records_.empty();
}

// =============================================================================
// ExtrapartitionQueue Implementation
// =============================================================================

ExtrapartitionQueue::ExtrapartitionQueue(TDQDefinition def)
    : definition_(std::move(def)) {
    statistics_.created = SystemClock::now();
}

ExtrapartitionQueue::~ExtrapartitionQueue() {
    if (is_open_) {
        (void)close();
    }
}

Result<void> ExtrapartitionQueue::open(TDQOpenMode mode) {
    std::unique_lock lock(mutex_);
    
    if (is_open_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "Queue already open");
    }
    
    std::ios_base::openmode flags = std::ios::binary;
    
    if (mode == TDQOpenMode::INPUT) {
        flags |= std::ios::in;
    } else {
        flags |= std::ios::out;
        if (definition_.file_append) {
            flags |= std::ios::app;
        }
    }
    
    file_.open(definition_.file_path, flags);
    
    if (!file_.is_open()) {
        return make_error<void>(ErrorCode::IO_ERROR,
            std::format("Failed to open file: {}", definition_.file_path.string()));
    }
    
    open_mode_ = mode;
    is_open_ = true;
    
    return {};
}

Result<void> ExtrapartitionQueue::close() {
    std::unique_lock lock(mutex_);
    
    if (!is_open_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "Queue not open");
    }
    
    file_.close();
    is_open_ = false;
    
    return {};
}

Result<void> ExtrapartitionQueue::write(ConstByteSpan data) {
    std::unique_lock lock(mutex_);
    
    if (!is_open_) {
        file_.open(definition_.file_path, 
                   std::ios::binary | std::ios::out | 
                   (definition_.file_append ? std::ios::app : std::ios::trunc));
        if (!file_.is_open()) {
            return make_error<void>(ErrorCode::IO_ERROR,
                std::format("Failed to open file: {}", definition_.file_path.string()));
        }
        open_mode_ = TDQOpenMode::OUTPUT;
        is_open_ = true;
    }
    
    if (open_mode_ != TDQOpenMode::OUTPUT) {
        return make_error<void>(ErrorCode::INVALID_STATE, "Queue not open for output");
    }
    
    if (definition_.record_length == 0) {
        UInt32 len = static_cast<UInt32>(data.size());
        file_.write(reinterpret_cast<const char*>(&len), sizeof(len));
    }
    
    file_.write(reinterpret_cast<const char*>(data.data()), 
                static_cast<std::streamsize>(data.size()));
    
    if (!file_) {
        return make_error<void>(ErrorCode::IO_ERROR, "Write failed");
    }
    
    file_.flush();
    statistics_.record_write(data.size());
    
    return {};
}

Result<void> ExtrapartitionQueue::write(StringView str) {
    return write(ConstByteSpan(reinterpret_cast<const Byte*>(str.data()), str.size()));
}

Result<TDQRecord> ExtrapartitionQueue::read() {
    std::unique_lock lock(mutex_);
    
    if (!is_open_) {
        file_.open(definition_.file_path, std::ios::binary | std::ios::in);
        if (!file_.is_open()) {
            return make_error<TDQRecord>(ErrorCode::IO_ERROR,
                std::format("Failed to open file: {}", definition_.file_path.string()));
        }
        open_mode_ = TDQOpenMode::INPUT;
        is_open_ = true;
    }
    
    if (open_mode_ != TDQOpenMode::INPUT) {
        return make_error<TDQRecord>(ErrorCode::INVALID_STATE, "Queue not open for input");
    }
    
    Size record_len = definition_.record_length;
    
    if (record_len == 0) {
        UInt32 len = 0;
        file_.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (file_.eof()) {
            return make_error<TDQRecord>(ErrorCode::VSAM_END_OF_FILE, "End of file");
        }
        if (!file_) {
            return make_error<TDQRecord>(ErrorCode::IO_ERROR, "Read length failed");
        }
        record_len = len;
    }
    
    ByteBuffer data(record_len);
    file_.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(record_len));
    
    if (file_.eof() && file_.gcount() == 0) {
        return make_error<TDQRecord>(ErrorCode::VSAM_END_OF_FILE, "End of file");
    }
    
    if (!file_ && !file_.eof()) {
        return make_error<TDQRecord>(ErrorCode::IO_ERROR, "Read failed");
    }
    
    statistics_.record_read(record_len);
    
    return TDQRecord(ConstByteSpan(data.data(), static_cast<Size>(file_.gcount())));
}

// =============================================================================
// TDQManager Implementation
// =============================================================================

TDQManager::TDQManager() = default;
TDQManager::~TDQManager() { shutdown(); }

TDQManager& TDQManager::instance() {
    static TDQManager manager;
    return manager;
}

Result<void> TDQManager::initialize() {
    std::unique_lock lock(mutex_);
    if (initialized_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "TDQ Manager already initialized");
    }
    initialized_ = true;
    return {};
}

void TDQManager::shutdown() {
    std::unique_lock lock(mutex_);
    if (!initialized_) return;
    intra_queues_.clear();
    extra_queues_.clear();
    indirect_map_.clear();
    initialized_ = false;
}

Result<String> TDQManager::resolve_destination(StringView dest) const {
    String dest_name = to_upper(String(dest));
    auto indirect_it = indirect_map_.find(dest_name);
    if (indirect_it != indirect_map_.end()) {
        return resolve_destination(indirect_it->second);
    }
    return dest_name;
}

Result<void> TDQManager::define_intrapartition(const TDQDefinition& def) {
    auto validation = def.validate();
    if (!validation) return validation;
    
    if (def.type != TDQType::INTRAPARTITION) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Definition type must be INTRAPARTITION");
    }
    
    std::unique_lock lock(mutex_);
    String dest_name = to_upper(def.dest_id.trimmed());
    
    if (intra_queues_.contains(dest_name) || extra_queues_.contains(dest_name)) {
        return make_error<void>(ErrorCode::FILE_EXISTS, std::format("Destination '{}' already defined", dest_name));
    }
    
    intra_queues_[dest_name] = make_unique<IntrapartitionQueue>(def);
    ++total_dests_defined_;
    return {};
}

Result<void> TDQManager::define_extrapartition(const TDQDefinition& def) {
    auto validation = def.validate();
    if (!validation) return validation;
    
    if (def.type != TDQType::EXTRAPARTITION) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Definition type must be EXTRAPARTITION");
    }
    
    std::unique_lock lock(mutex_);
    String dest_name = to_upper(def.dest_id.trimmed());
    
    if (intra_queues_.contains(dest_name) || extra_queues_.contains(dest_name)) {
        return make_error<void>(ErrorCode::FILE_EXISTS, std::format("Destination '{}' already defined", dest_name));
    }
    
    extra_queues_[dest_name] = make_unique<ExtrapartitionQueue>(def);
    ++total_dests_defined_;
    return {};
}

Result<void> TDQManager::define_indirect(StringView dest, StringView target) {
    std::unique_lock lock(mutex_);
    String dest_name = to_upper(String(dest));
    String target_name = to_upper(String(target));
    
    if (intra_queues_.contains(dest_name) || extra_queues_.contains(dest_name)) {
        return make_error<void>(ErrorCode::FILE_EXISTS, std::format("Destination '{}' already defined", dest_name));
    }
    
    indirect_map_[dest_name] = target_name;
    ++total_dests_defined_;
    return {};
}

Result<void> TDQManager::delete_destination(StringView dest) {
    std::unique_lock lock(mutex_);
    String dest_name = to_upper(String(dest));
    
    if (intra_queues_.erase(dest_name) > 0) return {};
    if (extra_queues_.erase(dest_name) > 0) return {};
    if (indirect_map_.erase(dest_name) > 0) return {};
    
    return make_error<void>(ErrorCode::CICS_QUEUE_NOT_FOUND, std::format("Destination '{}' not found", dest));
}

Result<void> TDQManager::writeq(StringView dest, ConstByteSpan data) {
    auto resolved = resolve_destination(dest);
    if (!resolved) return make_error<void>(resolved.error().code, resolved.error().message);
    
    String dest_name = resolved.value();
    std::shared_lock lock(mutex_);
    
    auto intra_it = intra_queues_.find(dest_name);
    if (intra_it != intra_queues_.end()) {
        ++total_writes_;
        return intra_it->second->write(data);
    }
    
    auto extra_it = extra_queues_.find(dest_name);
    if (extra_it != extra_queues_.end()) {
        ++total_writes_;
        return extra_it->second->write(data);
    }
    
    return make_error<void>(ErrorCode::CICS_QUEUE_NOT_FOUND, std::format("Destination '{}' not found", dest));
}

Result<void> TDQManager::writeq(StringView dest, StringView str) {
    return writeq(dest, ConstByteSpan(reinterpret_cast<const Byte*>(str.data()), str.size()));
}

Result<TDQRecord> TDQManager::readq(StringView dest) {
    auto resolved = resolve_destination(dest);
    if (!resolved) return make_error<TDQRecord>(resolved.error().code, resolved.error().message);
    
    String dest_name = resolved.value();
    std::shared_lock lock(mutex_);
    
    auto intra_it = intra_queues_.find(dest_name);
    if (intra_it != intra_queues_.end()) {
        ++total_reads_;
        return intra_it->second->read();
    }
    
    auto extra_it = extra_queues_.find(dest_name);
    if (extra_it != extra_queues_.end()) {
        ++total_reads_;
        return extra_it->second->read();
    }
    
    return make_error<TDQRecord>(ErrorCode::CICS_QUEUE_NOT_FOUND, std::format("Destination '{}' not found", dest));
}

Result<void> TDQManager::deleteq(StringView dest) { return delete_destination(dest); }

Result<void> TDQManager::enable_destination(StringView dest) {
    auto resolved = resolve_destination(dest);
    if (!resolved) return make_error<void>(resolved.error().code, resolved.error().message);
    
    String dest_name = resolved.value();
    std::shared_lock lock(mutex_);
    
    auto intra_it = intra_queues_.find(dest_name);
    if (intra_it != intra_queues_.end()) {
        intra_it->second->set_enabled(true);
        return {};
    }
    
    return make_error<void>(ErrorCode::CICS_QUEUE_NOT_FOUND, std::format("Destination '{}' not found", dest));
}

Result<void> TDQManager::disable_destination(StringView dest) {
    auto resolved = resolve_destination(dest);
    if (!resolved) return make_error<void>(resolved.error().code, resolved.error().message);
    
    String dest_name = resolved.value();
    std::shared_lock lock(mutex_);
    
    auto intra_it = intra_queues_.find(dest_name);
    if (intra_it != intra_queues_.end()) {
        intra_it->second->set_enabled(false);
        return {};
    }
    
    return make_error<void>(ErrorCode::CICS_QUEUE_NOT_FOUND, std::format("Destination '{}' not found", dest));
}

bool TDQManager::destination_exists(StringView dest) const {
    String dest_name = to_upper(String(dest));
    std::shared_lock lock(mutex_);
    return intra_queues_.contains(dest_name) || extra_queues_.contains(dest_name) || indirect_map_.contains(dest_name);
}

Size TDQManager::destination_count() const {
    std::shared_lock lock(mutex_);
    return intra_queues_.size() + extra_queues_.size() + indirect_map_.size();
}

std::vector<String> TDQManager::list_destinations() const {
    std::shared_lock lock(mutex_);
    std::vector<String> names;
    names.reserve(intra_queues_.size() + extra_queues_.size() + indirect_map_.size());
    for (const auto& [name, q] : intra_queues_) { (void)q; names.push_back(name); }
    for (const auto& [name, q] : extra_queues_) { (void)q; names.push_back(name); }
    for (const auto& [name, t] : indirect_map_) { (void)t; names.push_back(name); }
    return names;
}

Optional<TDQType> TDQManager::get_destination_type(StringView dest) const {
    String dest_name = to_upper(String(dest));
    std::shared_lock lock(mutex_);
    if (intra_queues_.contains(dest_name)) return TDQType::INTRAPARTITION;
    if (extra_queues_.contains(dest_name)) return TDQType::EXTRAPARTITION;
    if (indirect_map_.contains(dest_name)) return TDQType::INDIRECT;
    return std::nullopt;
}

Result<Size> TDQManager::get_queue_depth(StringView dest) const {
    auto resolved = resolve_destination(dest);
    if (!resolved) return make_error<Size>(resolved.error().code, resolved.error().message);
    
    String dest_name = resolved.value();
    std::shared_lock lock(mutex_);
    
    auto intra_it = intra_queues_.find(dest_name);
    if (intra_it != intra_queues_.end()) return intra_it->second->depth();
    
    return make_error<Size>(ErrorCode::CICS_QUEUE_NOT_FOUND, std::format("Destination '{}' not found or not intrapartition", dest));
}

String TDQManager::get_statistics() const {
    std::shared_lock lock(mutex_);
    std::ostringstream oss;
    oss << "TDQ Manager Statistics:\n"
        << "  Intrapartition Destinations: " << intra_queues_.size() << "\n"
        << "  Extrapartition Destinations: " << extra_queues_.size() << "\n"
        << "  Indirect Destinations: " << indirect_map_.size() << "\n"
        << "  Total Destinations Defined: " << total_dests_defined_.get() << "\n"
        << "  Total Writes: " << total_writes_.get() << "\n"
        << "  Total Reads: " << total_reads_.get();
    return oss.str();
}

// =============================================================================
// CICS Command Interface Implementation
// =============================================================================

Result<void> exec_cics_writeq_td(StringView queue, ConstByteSpan from) {
    return TDQManager::instance().writeq(queue, from);
}

Result<ByteBuffer> exec_cics_readq_td(StringView queue) {
    auto result = TDQManager::instance().readq(queue);
    if (!result) return make_error<ByteBuffer>(result.error().code, result.error().message);
    auto span = result.value().span();
    return ByteBuffer(span.begin(), span.end());
}

Result<void> exec_cics_deleteq_td(StringView queue) {
    return TDQManager::instance().deleteq(queue);
}

} // namespace cics::tdq
