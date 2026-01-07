// =============================================================================
// CICS Emulation - TSQ (Temporary Storage Queue) Example
// Version: 3.4.6
// =============================================================================
// Demonstrates WRITEQ TS, READQ TS, and DELETEQ TS operations
// =============================================================================

#include <iostream>
#include <iomanip>
#include <cics/tsq/tsq_types.hpp>

using namespace cics;
using namespace cics::tsq;

void print_separator(const char* title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

int main() {
    std::cout << R"(
+==============================================================+
|     CICS Emulation - TSQ Example Program          |
|                      Version 3.4.6                           |
+==============================================================+
)";

    // Initialize the TSQ Manager
    auto& mgr = TSQManager::instance();
    auto init_result = mgr.initialize("/tmp/tsq_auxiliary");
    if (!init_result) {
        std::cerr << "Failed to initialize TSQ Manager: " 
                  << init_result.error().message << "\n";
        return 1;
    }

    print_separator("1. WRITEQ TS - Writing Items to Queue");
    
    // Write several items to a queue named "SCRATCHQ"
    const char* queue_name = "SCRATCHQ";
    
    std::vector<String> messages = {
        "First message in the queue",
        "Second message - transaction data",
        "Third message - user session info",
        "Fourth message - application state"
    };
    
    for (const auto& msg : messages) {
        auto result = mgr.writeq(queue_name, msg, TSQLocation::MAIN);
        if (result) {
            std::cout << "  Written item #" << result.value() 
                      << ": \"" << msg.substr(0, 30) << "...\"\n";
        } else {
            std::cerr << "  Write failed: " << result.error().message << "\n";
        }
    }
    
    std::cout << "\n  Queue '" << queue_name << "' now has " 
              << mgr.queue_count() << " queue(s) active\n";

    print_separator("2. READQ TS - Reading Items by Number");
    
    // Read specific items by item number
    for (UInt32 i = 1; i <= 4; ++i) {
        auto result = mgr.readq(queue_name, i);
        if (result) {
            std::cout << "  Item #" << i << ": \"" 
                      << result.value().to_string() << "\"\n";
        } else {
            std::cerr << "  Read failed: " << result.error().message << "\n";
        }
    }

    print_separator("3. READQ TS NEXT - Sequential Reading");
    
    // Read items sequentially using read_next
    UInt32 cursor = 0;
    std::cout << "  Reading all items sequentially:\n";
    while (true) {
        auto result = mgr.readq_next(queue_name, cursor);
        if (!result) {
            if (result.error().code == ErrorCode::VSAM_END_OF_FILE) {
                std::cout << "  (End of queue reached)\n";
            } else {
                std::cerr << "  Error: " << result.error().message << "\n";
            }
            break;
        }
        std::cout << "    [" << cursor << "] " << result.value().to_string() << "\n";
    }

    print_separator("4. WRITEQ TS REWRITE - Updating an Item");
    
    // Rewrite item #2 with new data
    String updated_msg = "UPDATED: New transaction data for item 2";
    auto rewrite_result = mgr.rewriteq(queue_name, 2, updated_msg);
    if (rewrite_result) {
        std::cout << "  Successfully rewrote item #2\n";
        
        // Verify the update
        auto verify = mgr.readq(queue_name, 2);
        if (verify) {
            std::cout << "  Verified: \"" << verify.value().to_string() << "\"\n";
        }
    } else {
        std::cerr << "  Rewrite failed: " << rewrite_result.error().message << "\n";
    }

    print_separator("5. Working with Multiple Queues");
    
    // Create additional queues for different purposes
    mgr.writeq("USERDATA", "User profile information", TSQLocation::MAIN);
    mgr.writeq("USERDATA", "User preferences", TSQLocation::MAIN);
    mgr.writeq("TEMPWORK", "Temporary calculation results", TSQLocation::AUXILIARY);
    
    std::cout << "  Active queues:\n";
    for (const auto& name : mgr.list_queues()) {
        std::cout << "    - " << name << "\n";
    }

    print_separator("6. Queue Statistics");
    
    std::cout << mgr.get_statistics() << "\n";

    print_separator("7. DELETEQ TS - Deleting a Queue");
    
    // Delete individual item
    auto del_item = mgr.deleteq_item(queue_name, 1);
    if (del_item) {
        std::cout << "  Deleted item #1 from " << queue_name << "\n";
    }
    
    // Delete entire queue
    auto del_queue = mgr.deleteq("TEMPWORK");
    if (del_queue) {
        std::cout << "  Deleted queue TEMPWORK\n";
    }
    
    std::cout << "\n  Remaining queues: " << mgr.queue_count() << "\n";

    print_separator("8. Using EXEC CICS Interface");
    
    // Demonstrate the EXEC CICS style interface
    auto write_result = exec_cics_writeq_ts("CICSQ", 
        ConstByteSpan(reinterpret_cast<const Byte*>("EXEC CICS data"), 14),
        TSQLocation::MAIN, false, 0);
    
    if (write_result) {
        std::cout << "  EXEC CICS WRITEQ TS succeeded, item #" 
                  << write_result.value() << "\n";
        
        auto read_result = exec_cics_readq_ts("CICSQ", 1, false);
        if (read_result) {
            String data(read_result.value().begin(), read_result.value().end());
            std::cout << "  EXEC CICS READQ TS: \"" << data << "\"\n";
        }
    }

    // Cleanup
    mgr.shutdown();

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " TSQ Example completed successfully!\n";
    std::cout << std::string(60, '=') << "\n\n";

    return 0;
}
