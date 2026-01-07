// =============================================================================
// CICS Emulation - Temporary Storage Queue Implementation
// Version: 3.4.6
// =============================================================================

#include "cics/tsq/tsq_types.hpp"
#include <algorithm>
#include <format>
#include <sstream>

namespace cics::tsq {

// =============================================================================
// TSQItem Implementation
// =============================================================================

void TSQItem::set_data(ConstByteSpan data) {
    data_.assign(data.begin(), data.end());
    touch();
}

void TSQItem::set_data(StringView str) {
    data_.assign(str.begin(), str.end());
    touch();
}

String TSQItem::to_string() const {
    return String(reinterpret_cast<const char*>(data_.data()), data_.size());
}

// =============================================================================
// TSQDefinition Implementation
// =============================================================================

Result<void> TSQDefinition::validate() const {
    if (queue_name.empty()) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Queue name cannot be empty");
    }
    if (max_items == 0 || max_items > MAX_QUEUE_ITEMS) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, 
            std::format("Invalid max_items: {} (must be 1-{})", max_items, MAX_QUEUE_ITEMS));
    }
    if (max_item_length == 0 || max_item_length > MAX_ITEM_LENGTH) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT,
            std::format("Invalid max_item_length: {} (must be 1-{})", max_item_length, MAX_ITEM_LENGTH));
    }
    return {};
}

// =============================================================================
// TSQStatistics Implementation
// =============================================================================

TSQStatistics::TSQStatistics() 
    : created(SystemClock::now())
    , last_accessed(created) {}

void TSQStatistics::record_read() {
    ++reads;
    last_accessed = SystemClock::now();
}

void TSQStatistics::record_write(Size bytes) {
    ++writes;
    ++total_items;
    total_bytes += bytes;
    last_accessed = SystemClock::now();
}

void TSQStatistics::record_rewrite(Size old_bytes, Size new_bytes) {
    ++rewrites;
    if (new_bytes > old_bytes) {
        total_bytes += (new_bytes - old_bytes);
    } else {
        total_bytes -= (old_bytes - new_bytes);
    }
    last_accessed = SystemClock::now();
}

void TSQStatistics::record_delete(Size bytes) {
    ++deletes;
    --total_items;
    total_bytes -= bytes;
    last_accessed = SystemClock::now();
}

void TSQStatistics::update_peaks(UInt64 items, UInt64 bytes) {
    if (items > peak_items) peak_items = items;
    if (bytes > peak_bytes) peak_bytes = bytes;
}

String TSQStatistics::to_string() const {
    std::ostringstream oss;
    oss << "TSQ Statistics:\n"
        << "  Total Items: " << total_items.get() << " (peak: " << peak_items << ")\n"
        << "  Total Bytes: " << total_bytes.get() << " (peak: " << peak_bytes << ")\n"
        << "  Reads: " << reads.get() << "\n"
        << "  Writes: " << writes.get() << "\n"
        << "  Rewrites: " << rewrites.get() << "\n"
        << "  Deletes: " << deletes.get() << "\n"
        << "  DeleteQs: " << deleteqs.get();
    return oss.str();
}

String TSQStatistics::to_json() const {
    return std::format(R"({{"total_items":{},"total_bytes":{},"peak_items":{},"peak_bytes":{},"reads":{},"writes":{},"rewrites":{},"deletes":{},"deleteqs":{}}})",
        total_items.get(), total_bytes.get(), peak_items, peak_bytes,
        reads.get(), writes.get(), rewrites.get(), deletes.get(), deleteqs.get());
}

// =============================================================================
// TemporaryStorageQueue Implementation
// =============================================================================

TemporaryStorageQueue::TemporaryStorageQueue(TSQDefinition def)
    : definition_(std::move(def)) {
    statistics_.created = SystemClock::now();
}

Result<UInt32> TemporaryStorageQueue::write(ConstByteSpan data) {
    std::unique_lock lock(mutex_);
    
    if (deleted_) {
        return make_error<UInt32>(ErrorCode::INVALID_STATE, "Queue has been deleted");
    }
    
    if (data.size() > definition_.max_item_length) {
        return make_error<UInt32>(ErrorCode::INVALID_ARGUMENT,
            std::format("Item length {} exceeds maximum {}", data.size(), definition_.max_item_length));
    }
    
    if (items_.size() >= definition_.max_items) {
        return make_error<UInt32>(ErrorCode::RESOURCE_EXHAUSTED,
            std::format("Queue full: {} items", definition_.max_items));
    }
    
    UInt32 item_number = static_cast<UInt32>(items_.size() + 1);
    items_.emplace_back(data, item_number);
    
    statistics_.record_write(data.size());
    statistics_.update_peaks(items_.size(), statistics_.total_bytes.get());
    
    return item_number;
}

Result<UInt32> TemporaryStorageQueue::write(StringView str) {
    return write(ConstByteSpan(reinterpret_cast<const Byte*>(str.data()), str.size()));
}

Result<void> TemporaryStorageQueue::rewrite(UInt32 item_number, ConstByteSpan data) {
    std::unique_lock lock(mutex_);
    
    if (deleted_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "Queue has been deleted");
    }
    
    if (item_number == 0 || item_number > items_.size()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            std::format("Item {} not found (queue has {} items)", item_number, items_.size()));
    }
    
    if (data.size() > definition_.max_item_length) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT,
            std::format("Item length {} exceeds maximum {}", data.size(), definition_.max_item_length));
    }
    
    auto& item = items_[item_number - 1];
    Size old_size = item.length();
    item.set_data(data);
    
    statistics_.record_rewrite(old_size, data.size());
    statistics_.update_peaks(items_.size(), statistics_.total_bytes.get());
    
    return {};
}

Result<void> TemporaryStorageQueue::rewrite(UInt32 item_number, StringView str) {
    return rewrite(item_number, ConstByteSpan(reinterpret_cast<const Byte*>(str.data()), str.size()));
}

Result<TSQItem> TemporaryStorageQueue::read(UInt32 item_number) const {
    std::shared_lock lock(mutex_);
    
    if (deleted_) {
        return make_error<TSQItem>(ErrorCode::INVALID_STATE, "Queue has been deleted");
    }
    
    if (item_number == 0 || item_number > items_.size()) {
        return make_error<TSQItem>(ErrorCode::RECORD_NOT_FOUND,
            std::format("Item {} not found (queue has {} items)", item_number, items_.size()));
    }
    
    const_cast<TSQStatistics&>(statistics_).record_read();
    return items_[item_number - 1];
}

Result<TSQItem> TemporaryStorageQueue::read_next(UInt32& current_item) const {
    std::shared_lock lock(mutex_);
    
    if (deleted_) {
        return make_error<TSQItem>(ErrorCode::INVALID_STATE, "Queue has been deleted");
    }
    
    if (current_item >= items_.size()) {
        return make_error<TSQItem>(ErrorCode::VSAM_END_OF_FILE, "No more items in queue");
    }
    
    ++current_item;
    const_cast<TSQStatistics&>(statistics_).record_read();
    return items_[current_item - 1];
}

Result<void> TemporaryStorageQueue::delete_item(UInt32 item_number) {
    std::unique_lock lock(mutex_);
    
    if (deleted_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "Queue has been deleted");
    }
    
    if (item_number == 0 || item_number > items_.size()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            std::format("Item {} not found", item_number));
    }
    
    Size bytes = items_[item_number - 1].length();
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(item_number - 1));
    
    // Renumber remaining items
    for (UInt32 i = item_number - 1; i < items_.size(); ++i) {
        items_[i].set_item_number(i + 1);
    }
    
    statistics_.record_delete(bytes);
    
    return {};
}

Result<void> TemporaryStorageQueue::delete_all() {
    std::unique_lock lock(mutex_);
    
    if (deleted_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "Queue already deleted");
    }
    
    items_.clear();
    deleted_ = true;
    ++statistics_.deleteqs;
    statistics_.total_items.reset();
    statistics_.total_bytes.reset();
    
    return {};
}

Size TemporaryStorageQueue::item_count() const {
    std::shared_lock lock(mutex_);
    return items_.size();
}

Size TemporaryStorageQueue::total_bytes() const {
    std::shared_lock lock(mutex_);
    Size total = 0;
    for (const auto& item : items_) {
        total += item.length();
    }
    return total;
}

bool TemporaryStorageQueue::empty() const {
    std::shared_lock lock(mutex_);
    return items_.empty();
}

// =============================================================================
// TSQManager Implementation
// =============================================================================

TSQManager::TSQManager() 
    : auxiliary_threshold_(DEFAULT_AUXILIARY_THRESHOLD) {}

TSQManager::~TSQManager() {
    shutdown();
}

TSQManager& TSQManager::instance() {
    static TSQManager manager;
    return manager;
}

Result<void> TSQManager::initialize(const Path& auxiliary_path) {
    std::unique_lock lock(mutex_);
    
    if (initialized_) {
        return make_error<void>(ErrorCode::INVALID_STATE, "TSQ Manager already initialized");
    }
    
    auxiliary_storage_path_ = auxiliary_path;
    initialized_ = true;
    
    return {};
}

void TSQManager::shutdown() {
    std::unique_lock lock(mutex_);
    
    if (!initialized_) return;
    
    queues_.clear();
    initialized_ = false;
}

Result<TemporaryStorageQueue*> TSQManager::get_queue(StringView name) {
    std::shared_lock lock(mutex_);
    
    String queue_name = to_upper(String(name));
    auto it = queues_.find(queue_name);
    
    if (it == queues_.end()) {
        return make_error<TemporaryStorageQueue*>(ErrorCode::CICS_QUEUE_NOT_FOUND, 
            std::format("Queue '{}' not found", name));
    }
    
    if (it->second->is_deleted()) {
        return make_error<TemporaryStorageQueue*>(ErrorCode::CICS_QUEUE_NOT_FOUND,
            std::format("Queue '{}' has been deleted", name));
    }
    
    return it->second.get();
}

Result<TemporaryStorageQueue*> TSQManager::get_or_create_queue(StringView name, TSQLocation location) {
    String queue_name = to_upper(String(name));
    
    // First try read-only access
    {
        std::shared_lock lock(mutex_);
        auto it = queues_.find(queue_name);
        if (it != queues_.end() && !it->second->is_deleted()) {
            return it->second.get();
        }
    }
    
    // Need to create - upgrade to write lock
    std::unique_lock lock(mutex_);
    
    // Double-check after getting write lock
    auto it = queues_.find(queue_name);
    if (it != queues_.end() && !it->second->is_deleted()) {
        return it->second.get();
    }
    
    // Create new queue
    TSQDefinition def;
    def.queue_name = StringView(queue_name);
    def.location = location;
    
    auto queue = make_unique<TemporaryStorageQueue>(def);
    auto* ptr = queue.get();
    
    queues_[queue_name] = std::move(queue);
    ++total_queues_created_;
    ++active_queues_;
    
    return ptr;
}

Result<void> TSQManager::delete_queue(StringView name) {
    std::unique_lock lock(mutex_);
    
    String queue_name = to_upper(String(name));
    auto it = queues_.find(queue_name);
    
    if (it == queues_.end()) {
        return make_error<void>(ErrorCode::CICS_QUEUE_NOT_FOUND,
            std::format("Queue '{}' not found", name));
    }
    
    auto result = it->second->delete_all();
    if (!result) return result;
    
    queues_.erase(it);
    ++total_queues_deleted_;
    --active_queues_;
    
    return {};
}

bool TSQManager::queue_exists(StringView name) const {
    std::shared_lock lock(mutex_);
    String queue_name = to_upper(String(name));
    auto it = queues_.find(queue_name);
    return it != queues_.end() && !it->second->is_deleted();
}

Result<UInt32> TSQManager::writeq(StringView queue_name, ConstByteSpan data, TSQLocation location) {
    auto queue_result = get_or_create_queue(queue_name, location);
    if (!queue_result) {
        return make_error<UInt32>(queue_result.error().code, queue_result.error().message);
    }
    
    return queue_result.value()->write(data);
}

Result<UInt32> TSQManager::writeq(StringView queue_name, StringView str, TSQLocation location) {
    return writeq(queue_name, ConstByteSpan(reinterpret_cast<const Byte*>(str.data()), str.size()), location);
}

Result<void> TSQManager::rewriteq(StringView queue_name, UInt32 item_number, ConstByteSpan data) {
    auto queue_result = get_queue(queue_name);
    if (!queue_result) {
        return make_error<void>(queue_result.error().code, queue_result.error().message);
    }
    
    return queue_result.value()->rewrite(item_number, data);
}

Result<void> TSQManager::rewriteq(StringView queue_name, UInt32 item_number, StringView str) {
    return rewriteq(queue_name, item_number, 
                    ConstByteSpan(reinterpret_cast<const Byte*>(str.data()), str.size()));
}

Result<TSQItem> TSQManager::readq(StringView queue_name, UInt32 item_number) const {
    std::shared_lock lock(mutex_);
    
    String name = to_upper(String(queue_name));
    auto it = queues_.find(name);
    
    if (it == queues_.end() || it->second->is_deleted()) {
        return make_error<TSQItem>(ErrorCode::CICS_QUEUE_NOT_FOUND,
            std::format("Queue '{}' not found", queue_name));
    }
    
    return it->second->read(item_number);
}

Result<TSQItem> TSQManager::readq_next(StringView queue_name, UInt32& current_item) const {
    std::shared_lock lock(mutex_);
    
    String name = to_upper(String(queue_name));
    auto it = queues_.find(name);
    
    if (it == queues_.end() || it->second->is_deleted()) {
        return make_error<TSQItem>(ErrorCode::CICS_QUEUE_NOT_FOUND,
            std::format("Queue '{}' not found", queue_name));
    }
    
    return it->second->read_next(current_item);
}

Result<void> TSQManager::deleteq_item(StringView queue_name, UInt32 item_number) {
    auto queue_result = get_queue(queue_name);
    if (!queue_result) {
        return make_error<void>(queue_result.error().code, queue_result.error().message);
    }
    
    return queue_result.value()->delete_item(item_number);
}

Result<void> TSQManager::deleteq(StringView queue_name) {
    return delete_queue(queue_name);
}

Size TSQManager::queue_count() const {
    std::shared_lock lock(mutex_);
    return queues_.size();
}

std::vector<String> TSQManager::list_queues() const {
    std::shared_lock lock(mutex_);
    std::vector<String> names;
    names.reserve(queues_.size());
    for (const auto& [name, queue] : queues_) {
        if (!queue->is_deleted()) {
            names.push_back(name);
        }
    }
    return names;
}

std::vector<String> TSQManager::list_queues_by_prefix(StringView prefix) const {
    std::shared_lock lock(mutex_);
    String upper_prefix = to_upper(String(prefix));
    std::vector<String> names;
    
    for (const auto& [name, queue] : queues_) {
        if (!queue->is_deleted() && starts_with(name, upper_prefix)) {
            names.push_back(name);
        }
    }
    return names;
}

String TSQManager::get_statistics() const {
    std::shared_lock lock(mutex_);
    
    std::ostringstream oss;
    oss << "TSQ Manager Statistics:\n"
        << "  Active Queues: " << active_queues_.get() << "\n"
        << "  Total Created: " << total_queues_created_.get() << "\n"
        << "  Total Deleted: " << total_queues_deleted_.get() << "\n";
    
    UInt64 total_items = 0;
    UInt64 total_bytes = 0;
    
    for (const auto& [name, queue] : queues_) {
        if (!queue->is_deleted()) {
            total_items += queue->item_count();
            total_bytes += queue->total_bytes();
        }
    }
    
    oss << "  Total Items: " << total_items << "\n"
        << "  Total Bytes: " << total_bytes;
    
    return oss.str();
}

// =============================================================================
// CICS Command Interface Implementation
// =============================================================================

Result<UInt32> exec_cics_writeq_ts(StringView queue, ConstByteSpan from, 
                                    TSQLocation location, bool rewrite, UInt32 item) {
    auto& mgr = TSQManager::instance();
    
    if (rewrite && item > 0) {
        auto result = mgr.rewriteq(queue, item, from);
        if (!result) {
            return make_error<UInt32>(result.error().code, result.error().message);
        }
        return item;
    }
    
    return mgr.writeq(queue, from, location);
}

Result<ByteBuffer> exec_cics_readq_ts(StringView queue, UInt32 item, bool next) {
    auto& mgr = TSQManager::instance();
    
    if (next) {
        auto result = mgr.readq_next(queue, item);
        if (!result) {
            return make_error<ByteBuffer>(result.error().code, result.error().message);
        }
        auto span = result.value().span();
        return ByteBuffer(span.begin(), span.end());
    } else {
        auto result = mgr.readq(queue, item > 0 ? item : 1);
        if (!result) {
            return make_error<ByteBuffer>(result.error().code, result.error().message);
        }
        auto span = result.value().span();
        return ByteBuffer(span.begin(), span.end());
    }
}

Result<void> exec_cics_deleteq_ts(StringView queue, Optional<UInt32> item) {
    auto& mgr = TSQManager::instance();
    
    if (item.has_value()) {
        return mgr.deleteq_item(queue, item.value());
    }
    
    return mgr.deleteq(queue);
}

} // namespace cics::tsq
