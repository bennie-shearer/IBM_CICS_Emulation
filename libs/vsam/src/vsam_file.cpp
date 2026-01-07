#include "cics/vsam/vsam_types.hpp"
#include <fstream>

namespace cics::vsam {

VsamStatistics::VsamStatistics() : created(SystemClock::now()), last_accessed(created), last_modified(created) {}

void VsamStatistics::record_read(Duration time) {
    reads++;
    auto ns = std::chrono::duration_cast<Nanoseconds>(time).count();
    total_io_time_ns += ns;
    last_accessed = SystemClock::now();
}

void VsamStatistics::record_write(Duration time, Size bytes) {
    writes++; inserts++;
    used_bytes += bytes;
    auto ns = std::chrono::duration_cast<Nanoseconds>(time).count();
    total_io_time_ns += ns;
    last_modified = SystemClock::now();
    last_accessed = last_modified;
}

void VsamStatistics::record_delete() {
    deletes++;
    deleted_records++;
    last_modified = SystemClock::now();
}

void VsamStatistics::record_update(Duration time) {
    updates++;
    auto ns = std::chrono::duration_cast<Nanoseconds>(time).count();
    total_io_time_ns += ns;
    last_modified = SystemClock::now();
}

double VsamStatistics::space_utilization() const {
    return allocated_bytes > 0 ? static_cast<double>(used_bytes) * 100.0 / static_cast<double>(allocated_bytes) : 0.0;
}

double VsamStatistics::average_io_time_us() const {
    UInt64 total_ops = reads + writes + updates + deletes;
    return total_ops > 0 ? static_cast<double>(total_io_time_ns) / (static_cast<double>(total_ops) * 1000.0) : 0.0;
}

String VsamStatistics::to_string() const {
    return std::format("Records: {}, Reads: {}, Writes: {}, Utilization: {:.1f}%",
        record_count.get(), reads.get(), writes.get(), space_utilization());
}

String VsamStatistics::to_json() const {
    return std::format(R"({{"records":{},"reads":{},"writes":{},"utilization":{:.1f}}})",
        record_count.get(), reads.get(), writes.get(), space_utilization());
}

VsamRecord::VsamRecord(VsamKey key, ConstByteSpan data)
    : key_(std::move(key)), data_(data.begin(), data.end()), last_modified_(SystemClock::now()) {}

VsamRecord::VsamRecord(ConstByteSpan data, RBA rba)
    : data_(data.begin(), data.end()), last_modified_(SystemClock::now()) {
    address_.rba = rba;
}

VsamRecord::VsamRecord(ConstByteSpan data, RRN rrn)
    : data_(data.begin(), data.end()), last_modified_(SystemClock::now()) {
    address_.rrn = rrn;
}

void VsamRecord::set_data(ConstByteSpan data) {
    data_.assign(data.begin(), data.end());
    last_modified_ = SystemClock::now();
}

ByteBuffer VsamRecord::serialize() const {
    ByteBuffer buf;
    Size key_len = key_.length();
    Size data_len = data_.size();
    buf.reserve(sizeof(Size) * 2 + key_len + data_len + sizeof(VsamAddress));
    
    auto append = [&buf](const void* data, Size size) {
        const Byte* p = static_cast<const Byte*>(data);
        buf.insert(buf.end(), p, p + size);
    };
    
    append(&key_len, sizeof(Size));
    if (key_len > 0) append(key_.data(), key_len);
    append(&data_len, sizeof(Size));
    if (data_len > 0) append(data_.data(), data_len);
    append(&address_, sizeof(VsamAddress));
    
    return buf;
}

String VsamKey::to_hex() const {
    return to_hex_string(data_);
}

String VsamAddress::to_string() const {
    if (has_rba()) return std::format("RBA:{:016X}", rba);
    if (has_rrn()) return std::format("RRN:{}", rrn);
    return "INVALID";
}

BrowseContext::BrowseContext() : start_time_(Clock::now()) {
    browse_id_ = UUID::generate().to_string();
}

void BrowseContext::set_current(const VsamKey& key, const VsamAddress& addr) {
    current_key_ = key;
    current_address_ = addr;
    at_start_ = false;
}

void BrowseContext::reset() {
    current_key_ = VsamKey();
    current_address_ = VsamAddress();
    at_start_ = true;
    at_end_ = false;
}

Result<void> VsamDefinition::validate() const {
    if (cluster_name.empty()) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Cluster name required");
    }
    if (type == VsamType::KSDS && key_length == 0) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "KSDS requires key length");
    }
    if (ci_size < MIN_CI_SIZE || ci_size > MAX_CI_SIZE) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Invalid CI size");
    }
    if (maximum_record_length > MAX_RECORD_LENGTH) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Record length too large");
    }
    return make_success();
}

ControlInterval::ControlInterval(UInt32 num, UInt16 size)
    : ci_number(num), ci_size(size), free_space(size), record_count(0) {
    data.resize(size);
}

bool ControlInterval::has_space_for(Size record_size) const {
    return free_space >= record_size + sizeof(UInt16);  // Record + RDF
}

double ControlInterval::utilization() const {
    return ci_size > 0 ? static_cast<double>(ci_size - free_space) * 100.0 / ci_size : 0.0;
}

void ControlInterval::clear() {
    free_space = ci_size;
    record_count = 0;
    rdf.clear();
    std::fill(data.begin(), data.end(), 0);
}

} // namespace cics::vsam
