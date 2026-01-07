// =============================================================================
// CICS Emulation - Channel/Container Support
// =============================================================================
// Provides PUT CONTAINER, GET CONTAINER, DELETE CONTAINER functionality.
// Implements modern CICS data passing mechanism between programs.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#ifndef CICS_CHANNEL_HPP
#define CICS_CHANNEL_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <atomic>
#include <optional>

namespace cics {
namespace channel {

// =============================================================================
// Forward Declarations
// =============================================================================

class Channel;
class Container;
class ChannelManager;

// =============================================================================
// Constants
// =============================================================================

constexpr UInt32 MAX_CONTAINER_NAME_LENGTH = 16;
constexpr UInt32 MAX_CHANNEL_NAME_LENGTH = 16;
constexpr UInt32 MAX_CONTAINER_SIZE = 32 * 1024 * 1024;  // 32 MB

// =============================================================================
// Enumerations
// =============================================================================

enum class DataType {
    CHAR,           // Character data (DATATYPE(CHAR))
    BIT,            // Bit data (DATATYPE(BIT))
    DFHVALUE        // CICS internal value
};

enum class ContainerType {
    NORMAL,         // Normal container
    ERROR,          // Error container
    ABCODE,         // Abend code container
    ABDATA          // Abend data container
};

// =============================================================================
// Container Class
// =============================================================================

struct ContainerInfo {
    String name;
    UInt32 size;
    DataType data_type;
    ContainerType container_type;
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point modified;
};

class Container {
public:
    explicit Container(StringView name, DataType type = DataType::CHAR);
    
    // Properties
    [[nodiscard]] const String& name() const { return name_; }
    [[nodiscard]] DataType data_type() const { return data_type_; }
    [[nodiscard]] ContainerType container_type() const { return container_type_; }
    [[nodiscard]] UInt32 size() const { return static_cast<UInt32>(data_.size()); }
    [[nodiscard]] bool empty() const { return data_.empty(); }
    
    // Data access
    Result<void> put(const void* data, UInt32 length);
    Result<void> put(const ByteBuffer& data);
    Result<void> put(StringView str);
    
    Result<ByteBuffer> get() const;
    Result<UInt32> get(void* buffer, UInt32 max_length) const;
    Result<String> get_string() const;
    
    // Append data
    Result<void> append(const void* data, UInt32 length);
    
    // Replace specific portion
    Result<void> replace(UInt32 offset, const void* data, UInt32 length);
    
    // Clear contents
    void clear();
    
    // Information
    [[nodiscard]] ContainerInfo get_info() const;
    
    // Set container type
    void set_container_type(ContainerType type) { container_type_ = type; }
    
private:
    String name_;
    ByteBuffer data_;
    DataType data_type_;
    ContainerType container_type_ = ContainerType::NORMAL;
    std::chrono::steady_clock::time_point created_;
    std::chrono::steady_clock::time_point modified_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Channel Class
// =============================================================================

struct ChannelInfo {
    String name;
    UInt32 container_count;
    UInt64 total_size;
    std::chrono::steady_clock::time_point created;
};

class Channel {
public:
    explicit Channel(StringView name);
    
    // Properties
    [[nodiscard]] const String& name() const { return name_; }
    [[nodiscard]] UInt32 container_count() const;
    [[nodiscard]] UInt64 total_size() const;
    
    // Container operations
    Result<Container*> create_container(StringView name, DataType type = DataType::CHAR);
    Result<Container*> get_container(StringView name);
    [[nodiscard]] const Container* get_container(StringView name) const;
    Result<void> delete_container(StringView name);
    [[nodiscard]] bool has_container(StringView name) const;
    
    // Move container to another channel
    Result<void> move_container(StringView name, Channel& target);
    
    // Copy container to another channel
    Result<void> copy_container(StringView name, Channel& target, StringView new_name = "") const;
    
    // List containers
    [[nodiscard]] std::vector<String> list_containers() const;
    [[nodiscard]] std::vector<ContainerInfo> list_container_info() const;
    
    // Clear all containers
    void clear();
    
    // Information
    [[nodiscard]] ChannelInfo get_info() const;
    
private:
    String name_;
    std::unordered_map<String, std::unique_ptr<Container>> containers_;
    std::chrono::steady_clock::time_point created_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Channel Statistics
// =============================================================================

struct ChannelStats {
    UInt64 channels_created{0};
    UInt64 channels_deleted{0};
    UInt64 containers_created{0};
    UInt64 containers_deleted{0};
    UInt64 puts_executed{0};
    UInt64 gets_executed{0};
    UInt64 bytes_written{0};
    UInt64 bytes_read{0};
};

// =============================================================================
// Channel Manager
// =============================================================================

class ChannelManager {
public:
    static ChannelManager& instance();
    
    // Lifecycle
    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }
    
    // Channel operations
    Result<Channel*> create_channel(StringView name);
    Result<Channel*> get_channel(StringView name);
    Result<void> delete_channel(StringView name);
    [[nodiscard]] bool has_channel(StringView name) const;
    
    // Current channel (for implicit operations)
    void set_current_channel(StringView name);
    [[nodiscard]] Channel* current_channel();
    [[nodiscard]] const String& current_channel_name() const { return current_channel_name_; }
    
    // Container operations on current channel
    Result<void> put_container(StringView container, const void* data, UInt32 length);
    Result<void> put_container(StringView container, StringView channel, const void* data, UInt32 length);
    Result<ByteBuffer> get_container(StringView container);
    Result<ByteBuffer> get_container(StringView container, StringView channel);
    Result<void> delete_container(StringView container);
    Result<void> delete_container(StringView container, StringView channel);
    
    // Move/Copy between channels
    Result<void> move_container(StringView container, StringView from_channel, StringView to_channel);
    
    // List operations
    [[nodiscard]] std::vector<String> list_channels() const;
    [[nodiscard]] std::vector<ChannelInfo> list_channel_info() const;
    
    // Statistics
    [[nodiscard]] ChannelStats get_stats() const;
    void reset_stats();
    
private:
    ChannelManager() = default;
    ~ChannelManager() = default;
    ChannelManager(const ChannelManager&) = delete;
    ChannelManager& operator=(const ChannelManager&) = delete;
    
    bool initialized_ = false;
    std::unordered_map<String, std::unique_ptr<Channel>> channels_;
    thread_local static String current_channel_name_;
    ChannelStats stats_;
    mutable std::mutex mutex_;
};

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

// PUT CONTAINER
Result<void> exec_cics_put_container(StringView container, const void* data, UInt32 length);
Result<void> exec_cics_put_container(StringView container, StringView channel, 
                                      const void* data, UInt32 length);
Result<void> exec_cics_put_container(StringView container, const ByteBuffer& data);
Result<void> exec_cics_put_container(StringView container, StringView str);
Result<void> exec_cics_put_container_channel(StringView container, StringView channel,
                                              const ByteBuffer& data);

// GET CONTAINER
Result<ByteBuffer> exec_cics_get_container(StringView container);
Result<ByteBuffer> exec_cics_get_container(StringView container, StringView channel);
Result<UInt32> exec_cics_get_container_into(StringView container, void* buffer, UInt32 max_length);
Result<UInt32> exec_cics_get_container_set(StringView container, ByteBuffer& data);

// DELETE CONTAINER
Result<void> exec_cics_delete_container(StringView container);
Result<void> exec_cics_delete_container(StringView container, StringView channel);

// MOVE CONTAINER
Result<void> exec_cics_move_container(StringView container, StringView from_channel, 
                                       StringView to_channel);

// CREATE CHANNEL
Result<void> exec_cics_create_channel(StringView channel);

// DELETE CHANNEL
Result<void> exec_cics_delete_channel(StringView channel);

// STARTBROWSE CONTAINER
Result<std::vector<String>> exec_cics_browse_containers(StringView channel);

// =============================================================================
// Utility Functions
// =============================================================================

[[nodiscard]] String data_type_to_string(DataType type);
[[nodiscard]] String container_type_to_string(ContainerType type);

} // namespace channel
} // namespace cics

#endif // CICS_CHANNEL_HPP
