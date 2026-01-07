// =============================================================================
// CICS Emulation - Channel/Container Implementation
// =============================================================================

#include <cics/channel/channel.hpp>
#include <algorithm>
#include <cstring>

namespace cics {
namespace channel {

// Thread-local current channel
thread_local String ChannelManager::current_channel_name_;

// =============================================================================
// Utility Functions
// =============================================================================

String data_type_to_string(DataType type) {
    switch (type) {
        case DataType::CHAR: return "CHAR";
        case DataType::BIT: return "BIT";
        case DataType::DFHVALUE: return "DFHVALUE";
        default: return "UNKNOWN";
    }
}

String container_type_to_string(ContainerType type) {
    switch (type) {
        case ContainerType::NORMAL: return "NORMAL";
        case ContainerType::ERROR: return "ERROR";
        case ContainerType::ABCODE: return "ABCODE";
        case ContainerType::ABDATA: return "ABDATA";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// Container Implementation
// =============================================================================

Container::Container(StringView name, DataType type)
    : name_(name)
    , data_type_(type)
    , created_(std::chrono::steady_clock::now())
    , modified_(created_)
{
}

Result<void> Container::put(const void* data, UInt32 length) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (length > MAX_CONTAINER_SIZE) {
        return make_error<void>(ErrorCode::LENGERR,
            "Container data exceeds maximum size");
    }
    
    data_.resize(length);
    if (length > 0 && data) {
        std::memcpy(data_.data(), data, length);
    }
    modified_ = std::chrono::steady_clock::now();
    
    return make_success();
}

Result<void> Container::put(const ByteBuffer& data) {
    return put(data.data(), static_cast<UInt32>(data.size()));
}

Result<void> Container::put(StringView str) {
    return put(str.data(), static_cast<UInt32>(str.size()));
}

Result<ByteBuffer> Container::get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return make_success(data_);
}

Result<UInt32> Container::get(void* buffer, UInt32 max_length) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!buffer) {
        return make_error<UInt32>(ErrorCode::INVREQ, "Null buffer provided");
    }
    
    UInt32 copy_length = std::min(max_length, static_cast<UInt32>(data_.size()));
    if (copy_length > 0) {
        std::memcpy(buffer, data_.data(), copy_length);
    }
    
    return make_success(copy_length);
}

Result<String> Container::get_string() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return make_success(String(reinterpret_cast<const char*>(data_.data()), data_.size()));
}

Result<void> Container::append(const void* data, UInt32 length) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (data_.size() + length > MAX_CONTAINER_SIZE) {
        return make_error<void>(ErrorCode::LENGERR,
            "Appended data would exceed maximum container size");
    }
    
    if (length > 0 && data) {
        size_t old_size = data_.size();
        data_.resize(old_size + length);
        std::memcpy(data_.data() + old_size, data, length);
    }
    modified_ = std::chrono::steady_clock::now();
    
    return make_success();
}

Result<void> Container::replace(UInt32 offset, const void* data, UInt32 length) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (offset + length > data_.size()) {
        return make_error<void>(ErrorCode::LENGERR,
            "Replace operation exceeds container bounds");
    }
    
    if (length > 0 && data) {
        std::memcpy(data_.data() + offset, data, length);
    }
    modified_ = std::chrono::steady_clock::now();
    
    return make_success();
}

void Container::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
    modified_ = std::chrono::steady_clock::now();
}

ContainerInfo Container::get_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ContainerInfo info;
    info.name = name_;
    info.size = static_cast<UInt32>(data_.size());
    info.data_type = data_type_;
    info.container_type = container_type_;
    info.created = created_;
    info.modified = modified_;
    return info;
}

// =============================================================================
// Channel Implementation
// =============================================================================

Channel::Channel(StringView name)
    : name_(name)
    , created_(std::chrono::steady_clock::now())
{
}

UInt32 Channel::container_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<UInt32>(containers_.size());
}

UInt64 Channel::total_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    UInt64 total = 0;
    for (const auto& [name, container] : containers_) {
        total += container->size();
    }
    return total;
}

Result<Container*> Channel::create_container(StringView name, DataType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (name.length() > MAX_CONTAINER_NAME_LENGTH) {
        return make_error<Container*>(ErrorCode::INVREQ,
            "Container name exceeds maximum length");
    }
    
    String key(name);
    auto it = containers_.find(key);
    if (it != containers_.end()) {
        // Return existing container
        return make_success(it->second.get());
    }
    
    auto container = std::make_unique<Container>(name, type);
    Container* ptr = container.get();
    containers_[key] = std::move(container);
    
    return make_success(ptr);
}

Result<Container*> Channel::get_container(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = containers_.find(String(name));
    if (it == containers_.end()) {
        return make_error<Container*>(ErrorCode::CONTAINERERR,
            "Container not found: " + String(name));
    }
    
    return make_success(it->second.get());
}

const Container* Channel::get_container(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = containers_.find(String(name));
    if (it == containers_.end()) {
        return nullptr;
    }
    
    return it->second.get();
}

Result<void> Channel::delete_container(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = containers_.find(String(name));
    if (it == containers_.end()) {
        return make_error<void>(ErrorCode::CONTAINERERR,
            "Container not found: " + String(name));
    }
    
    containers_.erase(it);
    return make_success();
}

bool Channel::has_container(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return containers_.find(String(name)) != containers_.end();
}

Result<void> Channel::move_container(StringView name, Channel& target) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = containers_.find(String(name));
    if (it == containers_.end()) {
        return make_error<void>(ErrorCode::CONTAINERERR,
            "Container not found: " + String(name));
    }
    
    // Move to target
    {
        std::lock_guard<std::mutex> target_lock(target.mutex_);
        target.containers_[String(name)] = std::move(it->second);
    }
    
    containers_.erase(it);
    return make_success();
}

Result<void> Channel::copy_container(StringView name, Channel& target, StringView new_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = containers_.find(String(name));
    if (it == containers_.end()) {
        return make_error<void>(ErrorCode::CONTAINERERR,
            "Container not found: " + String(name));
    }
    
    String target_name = new_name.empty() ? String(name) : String(new_name);
    
    // Create copy in target
    auto result = target.create_container(target_name, it->second->data_type());
    if (result.is_error()) {
        return make_error<void>(result.error().code, result.error().message);
    }
    
    auto data = it->second->get();
    if (data.is_success()) {
        result.value()->put(data.value());
    }
    
    return make_success();
}

std::vector<String> Channel::list_containers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<String> names;
    names.reserve(containers_.size());
    for (const auto& [name, container] : containers_) {
        names.push_back(name);
    }
    return names;
}

std::vector<ContainerInfo> Channel::list_container_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ContainerInfo> infos;
    infos.reserve(containers_.size());
    for (const auto& [name, container] : containers_) {
        infos.push_back(container->get_info());
    }
    return infos;
}

void Channel::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    containers_.clear();
}

ChannelInfo Channel::get_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ChannelInfo info;
    info.name = name_;
    info.container_count = static_cast<UInt32>(containers_.size());
    info.total_size = 0;
    for (const auto& [name, container] : containers_) {
        info.total_size += container->size();
    }
    info.created = created_;
    return info;
}

// =============================================================================
// ChannelManager Implementation
// =============================================================================

ChannelManager& ChannelManager::instance() {
    static ChannelManager instance;
    return instance;
}

void ChannelManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;
    
    channels_.clear();
    reset_stats();
    initialized_ = true;
}

void ChannelManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    channels_.clear();
    current_channel_name_.clear();
    initialized_ = false;
}

Result<Channel*> ChannelManager::create_channel(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return make_error<Channel*>(ErrorCode::NOT_INITIALIZED,
            "ChannelManager not initialized");
    }
    
    if (name.length() > MAX_CHANNEL_NAME_LENGTH) {
        return make_error<Channel*>(ErrorCode::INVREQ,
            "Channel name exceeds maximum length");
    }
    
    String key(name);
    auto it = channels_.find(key);
    if (it != channels_.end()) {
        return make_success(it->second.get());
    }
    
    auto channel = std::make_unique<Channel>(name);
    Channel* ptr = channel.get();
    channels_[key] = std::move(channel);
    
    ++stats_.channels_created;
    return make_success(ptr);
}

Result<Channel*> ChannelManager::get_channel(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = channels_.find(String(name));
    if (it == channels_.end()) {
        return make_error<Channel*>(ErrorCode::CHANNELERR,
            "Channel not found: " + String(name));
    }
    
    return make_success(it->second.get());
}

Result<void> ChannelManager::delete_channel(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = channels_.find(String(name));
    if (it == channels_.end()) {
        return make_error<void>(ErrorCode::CHANNELERR,
            "Channel not found: " + String(name));
    }
    
    channels_.erase(it);
    ++stats_.channels_deleted;
    
    if (current_channel_name_ == name) {
        current_channel_name_.clear();
    }
    
    return make_success();
}

bool ChannelManager::has_channel(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return channels_.find(String(name)) != channels_.end();
}

void ChannelManager::set_current_channel(StringView name) {
    current_channel_name_ = String(name);
}

Channel* ChannelManager::current_channel() {
    if (current_channel_name_.empty()) {
        return nullptr;
    }
    
    auto result = get_channel(current_channel_name_);
    return result.is_success() ? result.value() : nullptr;
}

Result<void> ChannelManager::put_container(StringView container, const void* data, UInt32 length) {
    Channel* channel = current_channel();
    if (!channel) {
        // Create default channel
        auto result = create_channel("DFHCNV");
        if (result.is_error()) {
            return make_error<void>(result.error().code, result.error().message);
        }
        channel = result.value();
        set_current_channel("DFHCNV");
    }
    
    auto cont_result = channel->create_container(container);
    if (cont_result.is_error()) {
        return make_error<void>(cont_result.error().code, cont_result.error().message);
    }
    
    auto put_result = cont_result.value()->put(data, length);
    if (put_result.is_success()) {
        ++stats_.puts_executed;
        stats_.bytes_written += length;
        ++stats_.containers_created;
    }
    return put_result;
}

Result<void> ChannelManager::put_container(StringView container, StringView channel_name,
                                            const void* data, UInt32 length) {
    auto channel_result = get_channel(channel_name);
    if (channel_result.is_error()) {
        // Auto-create channel
        channel_result = create_channel(channel_name);
        if (channel_result.is_error()) {
            return make_error<void>(channel_result.error().code, channel_result.error().message);
        }
    }
    
    Channel* channel = channel_result.value();
    auto cont_result = channel->create_container(container);
    if (cont_result.is_error()) {
        return make_error<void>(cont_result.error().code, cont_result.error().message);
    }
    
    auto put_result = cont_result.value()->put(data, length);
    if (put_result.is_success()) {
        ++stats_.puts_executed;
        stats_.bytes_written += length;
    }
    return put_result;
}

Result<ByteBuffer> ChannelManager::get_container(StringView container) {
    Channel* channel = current_channel();
    if (!channel) {
        return make_error<ByteBuffer>(ErrorCode::CHANNELERR, "No current channel");
    }
    
    auto cont_result = channel->get_container(container);
    if (cont_result.is_error()) {
        return make_error<ByteBuffer>(cont_result.error().code, cont_result.error().message);
    }
    
    auto data = cont_result.value()->get();
    if (data.is_success()) {
        ++stats_.gets_executed;
        stats_.bytes_read += data.value().size();
    }
    return data;
}

Result<ByteBuffer> ChannelManager::get_container(StringView container, StringView channel_name) {
    auto channel_result = get_channel(channel_name);
    if (channel_result.is_error()) {
        return make_error<ByteBuffer>(channel_result.error().code, channel_result.error().message);
    }
    
    auto cont_result = channel_result.value()->get_container(container);
    if (cont_result.is_error()) {
        return make_error<ByteBuffer>(cont_result.error().code, cont_result.error().message);
    }
    
    auto data = cont_result.value()->get();
    if (data.is_success()) {
        ++stats_.gets_executed;
        stats_.bytes_read += data.value().size();
    }
    return data;
}

Result<void> ChannelManager::delete_container(StringView container) {
    Channel* channel = current_channel();
    if (!channel) {
        return make_error<void>(ErrorCode::CHANNELERR, "No current channel");
    }
    
    auto result = channel->delete_container(container);
    if (result.is_success()) {
        ++stats_.containers_deleted;
    }
    return result;
}

Result<void> ChannelManager::delete_container(StringView container, StringView channel_name) {
    auto channel_result = get_channel(channel_name);
    if (channel_result.is_error()) {
        return make_error<void>(channel_result.error().code, channel_result.error().message);
    }
    
    auto result = channel_result.value()->delete_container(container);
    if (result.is_success()) {
        ++stats_.containers_deleted;
    }
    return result;
}

Result<void> ChannelManager::move_container(StringView container, StringView from_channel,
                                             StringView to_channel) {
    auto from_result = get_channel(from_channel);
    if (from_result.is_error()) {
        return make_error<void>(from_result.error().code, from_result.error().message);
    }
    
    auto to_result = get_channel(to_channel);
    if (to_result.is_error()) {
        // Auto-create target channel
        to_result = create_channel(to_channel);
        if (to_result.is_error()) {
            return make_error<void>(to_result.error().code, to_result.error().message);
        }
    }
    
    return from_result.value()->move_container(container, *to_result.value());
}

std::vector<String> ChannelManager::list_channels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<String> names;
    names.reserve(channels_.size());
    for (const auto& [name, channel] : channels_) {
        names.push_back(name);
    }
    return names;
}

std::vector<ChannelInfo> ChannelManager::list_channel_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ChannelInfo> infos;
    infos.reserve(channels_.size());
    for (const auto& [name, channel] : channels_) {
        infos.push_back(channel->get_info());
    }
    return infos;
}

ChannelStats ChannelManager::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void ChannelManager::reset_stats() {
    stats_.channels_created = 0;
    stats_.channels_deleted = 0;
    stats_.containers_created = 0;
    stats_.containers_deleted = 0;
    stats_.puts_executed = 0;
    stats_.gets_executed = 0;
    stats_.bytes_written = 0;
    stats_.bytes_read = 0;
}

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

Result<void> exec_cics_put_container(StringView container, const void* data, UInt32 length) {
    return ChannelManager::instance().put_container(container, data, length);
}

Result<void> exec_cics_put_container(StringView container, StringView channel,
                                      const void* data, UInt32 length) {
    return ChannelManager::instance().put_container(container, channel, data, length);
}

Result<void> exec_cics_put_container(StringView container, const ByteBuffer& data) {
    return ChannelManager::instance().put_container(container, data.data(),
                                                     static_cast<UInt32>(data.size()));
}

Result<void> exec_cics_put_container(StringView container, StringView str) {
    return ChannelManager::instance().put_container(container, str.data(),
                                                     static_cast<UInt32>(str.size()));
}

Result<void> exec_cics_put_container_channel(StringView container, StringView channel,
                                              const ByteBuffer& data) {
    return ChannelManager::instance().put_container(container, channel, data.data(),
                                                     static_cast<UInt32>(data.size()));
}

Result<ByteBuffer> exec_cics_get_container(StringView container) {
    return ChannelManager::instance().get_container(container);
}

Result<ByteBuffer> exec_cics_get_container(StringView container, StringView channel) {
    return ChannelManager::instance().get_container(container, channel);
}

Result<UInt32> exec_cics_get_container_into(StringView container, void* buffer, UInt32 max_length) {
    auto data = ChannelManager::instance().get_container(container);
    if (data.is_error()) {
        return make_error<UInt32>(data.error().code, data.error().message);
    }
    
    UInt32 copy_len = std::min(max_length, static_cast<UInt32>(data.value().size()));
    if (copy_len > 0) {
        std::memcpy(buffer, data.value().data(), copy_len);
    }
    return make_success(copy_len);
}

Result<UInt32> exec_cics_get_container_set(StringView container, ByteBuffer& data) {
    auto result = ChannelManager::instance().get_container(container);
    if (result.is_error()) {
        return make_error<UInt32>(result.error().code, result.error().message);
    }
    
    data = result.value();
    return make_success(static_cast<UInt32>(data.size()));
}

Result<void> exec_cics_delete_container(StringView container) {
    return ChannelManager::instance().delete_container(container);
}

Result<void> exec_cics_delete_container(StringView container, StringView channel) {
    return ChannelManager::instance().delete_container(container, channel);
}

Result<void> exec_cics_move_container(StringView container, StringView from_channel,
                                       StringView to_channel) {
    return ChannelManager::instance().move_container(container, from_channel, to_channel);
}

Result<void> exec_cics_create_channel(StringView channel) {
    auto result = ChannelManager::instance().create_channel(channel);
    if (result.is_error()) {
        return make_error<void>(result.error().code, result.error().message);
    }
    return make_success();
}

Result<void> exec_cics_delete_channel(StringView channel) {
    return ChannelManager::instance().delete_channel(channel);
}

Result<std::vector<String>> exec_cics_browse_containers(StringView channel) {
    auto result = ChannelManager::instance().get_channel(channel);
    if (result.is_error()) {
        return make_error<std::vector<String>>(result.error().code, result.error().message);
    }
    
    return make_success(result.value()->list_containers());
}

} // namespace channel
} // namespace cics
