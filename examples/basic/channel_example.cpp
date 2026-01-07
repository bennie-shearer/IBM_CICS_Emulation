// =============================================================================
// CICS Emulation - Channel/Container Example
// =============================================================================
// Demonstrates PUT CONTAINER, GET CONTAINER, and channel operations.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#include <cics/channel/channel.hpp>
#include <iostream>
#include <iomanip>

using namespace cics;
using namespace cics::channel;

void print_header(const char* title) {
    std::cout << "\n============================================================\n";
    std::cout << " " << title << "\n";
    std::cout << "============================================================\n";
}

int main() {
    std::cout << "\n";
    std::cout << "+==============================================================+\n";
    std::cout << "|     CICS Emulation - Channel/Container Example    |\n";
    std::cout << "|                        Version 3.4.6                         |\n";
    std::cout << "+==============================================================+\n";

    // Initialize
    ChannelManager::instance().initialize();

    // =========================================================================
    // 1. Create Channel and Containers
    // =========================================================================
    print_header("1. Create Channel and Containers");
    
    auto channel_result = ChannelManager::instance().create_channel("MYCHANNEL");
    if (channel_result.is_success()) {
        std::cout << "  Created channel: MYCHANNEL\n";
        
        Channel* channel = channel_result.value();
        
        // Create containers
        auto c1 = channel->create_container("INPUT-DATA");
        auto c2 = channel->create_container("OUTPUT-DATA");
        auto c3 = channel->create_container("CONFIG", DataType::CHAR);
        
        std::cout << "  Created containers: INPUT-DATA, OUTPUT-DATA, CONFIG\n";
        std::cout << "  Container count: " << channel->container_count() << "\n";
    }

    // =========================================================================
    // 2. PUT CONTAINER
    // =========================================================================
    print_header("2. PUT CONTAINER Operations");
    
    // Set current channel
    ChannelManager::instance().set_current_channel("MYCHANNEL");
    
    // Put data into containers
    String input_data = "Customer Order #12345";
    auto put_result = exec_cics_put_container("INPUT-DATA", input_data);
    if (put_result.is_success()) {
        std::cout << "  PUT to INPUT-DATA: \"" << input_data << "\"\n";
    }
    
    String config_data = "DEBUG=true;TIMEOUT=30";
    put_result = exec_cics_put_container("CONFIG", config_data);
    if (put_result.is_success()) {
        std::cout << "  PUT to CONFIG: \"" << config_data << "\"\n";
    }
    
    // Put binary data
    ByteBuffer binary_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    put_result = exec_cics_put_container("OUTPUT-DATA", binary_data);
    if (put_result.is_success()) {
        std::cout << "  PUT to OUTPUT-DATA: [5 bytes binary data]\n";
    }

    // =========================================================================
    // 3. GET CONTAINER
    // =========================================================================
    print_header("3. GET CONTAINER Operations");
    
    auto get_result = exec_cics_get_container("INPUT-DATA");
    if (get_result.is_success()) {
        String value(reinterpret_cast<const char*>(get_result.value().data()), 
                     get_result.value().size());
        std::cout << "  GET from INPUT-DATA: \"" << value << "\"\n";
        std::cout << "  Size: " << get_result.value().size() << " bytes\n";
    }
    
    get_result = exec_cics_get_container("CONFIG");
    if (get_result.is_success()) {
        String value(reinterpret_cast<const char*>(get_result.value().data()),
                     get_result.value().size());
        std::cout << "  GET from CONFIG: \"" << value << "\"\n";
    }

    // =========================================================================
    // 4. Cross-Channel Operations
    // =========================================================================
    print_header("4. Cross-Channel Operations");
    
    // Create another channel
    auto channel2_result = exec_cics_create_channel("RESPONSE");
    if (channel2_result.is_success()) {
        std::cout << "  Created channel: RESPONSE\n";
    }
    
    // Put with explicit channel
    String response = "Order processed successfully";
    put_result = exec_cics_put_container("STATUS", "RESPONSE",
                                          response.data(), static_cast<UInt32>(response.size()));
    if (put_result.is_success()) {
        std::cout << "  PUT to RESPONSE/STATUS: \"" << response << "\"\n";
    }
    
    // Get from explicit channel
    get_result = exec_cics_get_container("STATUS", "RESPONSE");
    if (get_result.is_success()) {
        String value(reinterpret_cast<const char*>(get_result.value().data()),
                     get_result.value().size());
        std::cout << "  GET from RESPONSE/STATUS: \"" << value << "\"\n";
    }

    // =========================================================================
    // 5. Browse Containers
    // =========================================================================
    print_header("5. Browse Containers");
    
    auto browse_result = exec_cics_browse_containers("MYCHANNEL");
    if (browse_result.is_success()) {
        std::cout << "  Containers in MYCHANNEL:\n";
        for (const auto& name : browse_result.value()) {
            std::cout << "    - " << name << "\n";
        }
    }

    // =========================================================================
    // 6. Delete Container
    // =========================================================================
    print_header("6. Delete Container");
    
    auto delete_result = exec_cics_delete_container("CONFIG");
    if (delete_result.is_success()) {
        std::cout << "  Deleted container: CONFIG\n";
    }
    
    browse_result = exec_cics_browse_containers("MYCHANNEL");
    if (browse_result.is_success()) {
        std::cout << "  Remaining containers: " << browse_result.value().size() << "\n";
    }

    // =========================================================================
    // 7. Statistics
    // =========================================================================
    print_header("7. Channel Statistics");
    
    auto stats = ChannelManager::instance().get_stats();
    std::cout << "  Channels Created:    " << stats.channels_created << "\n";
    std::cout << "  Containers Created:  " << stats.containers_created << "\n";
    std::cout << "  PUTs Executed:       " << stats.puts_executed << "\n";
    std::cout << "  GETs Executed:       " << stats.gets_executed << "\n";
    std::cout << "  Bytes Written:       " << stats.bytes_written << "\n";
    std::cout << "  Bytes Read:          " << stats.bytes_read << "\n";

    // Cleanup
    ChannelManager::instance().shutdown();

    std::cout << "\n================================================================\n";
    std::cout << "Channel/Container example completed successfully!\n";
    std::cout << "================================================================\n\n";

    return 0;
}
