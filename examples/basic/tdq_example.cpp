// =============================================================================
// CICS Emulation - TDQ (Transient Data Queue) Example
// Version: 3.4.6
// =============================================================================
// Demonstrates WRITEQ TD, READQ TD, and queue management operations
// =============================================================================

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cics/tdq/tdq_types.hpp>

using namespace cics;
using namespace cics::tdq;

void print_separator(const char* title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

// Example ATI callback function
void ati_callback(StringView transaction_id, StringView dest_id) {
    std::cout << "  [ATI TRIGGERED] Transaction: " << transaction_id 
              << " for destination: " << dest_id << "\n";
}

int main() {
    std::cout << R"(
+==============================================================+
|     CICS Emulation - TDQ Example Program          |
|                      Version 3.4.6                           |
+==============================================================+
)";

    // Initialize the TDQ Manager
    auto& mgr = TDQManager::instance();
    auto init_result = mgr.initialize();
    if (!init_result) {
        std::cerr << "Failed to initialize TDQ Manager: " 
                  << init_result.error().message << "\n";
        return 1;
    }

    print_separator("1. Defining Intrapartition Destinations");
    
    // Define an intrapartition destination (memory-based queue)
    TDQDefinition intra_def;
    intra_def.dest_id = StringView("CSL");  // CICS System Log (3 chars to fit FixedString<4>)
    intra_def.type = TDQType::INTRAPARTITION;
    intra_def.disposition = TDQDisposition::DELETE;
    intra_def.max_records = 1000;
    
    auto def_result = mgr.define_intrapartition(intra_def);
    if (def_result) {
        std::cout << "  Defined intrapartition destination: CSL\n";
    } else {
        std::cerr << "  Failed: " << def_result.error().message << "\n";
    }
    
    // Define another with ATI (Automatic Transaction Initiation)
    TDQDefinition ati_def;
    ati_def.dest_id = StringView("MSG");  // Message Log
    ati_def.type = TDQType::INTRAPARTITION;
    ati_def.disposition = TDQDisposition::DELETE;
    
    // Set up ATI trigger
    TriggerDefinition trigger;
    trigger.enabled = true;
    trigger.trigger_level = 3;  // Trigger after 3 records
    trigger.transaction_id = StringView("MSGP");  // Transaction to initiate
    trigger.callback = ati_callback;
    ati_def.trigger = trigger;
    
    mgr.define_intrapartition(ati_def);
    std::cout << "  Defined intrapartition destination with ATI: MSG\n";
    std::cout << "    (ATI triggers after 3 records to start MSGP transaction)\n";

    print_separator("2. Defining Extrapartition Destinations");
    
    // Define an extrapartition destination (file-based queue)
    TDQDefinition extra_def;
    extra_def.dest_id = StringView("PRT");  // Print output
    extra_def.type = TDQType::EXTRAPARTITION;
    extra_def.file_path = "/tmp/cics_print_output.txt";
    extra_def.file_append = true;
    extra_def.record_length = 0;  // Variable length records
    
    auto extra_result = mgr.define_extrapartition(extra_def);
    if (extra_result) {
        std::cout << "  Defined extrapartition destination: PRT\n";
        std::cout << "    Output file: /tmp/cics_print_output.txt\n";
    }

    print_separator("3. Defining Indirect Destinations");
    
    // Define an indirect destination (routes to another destination)
    auto indirect_result = mgr.define_indirect("LOG", "CSL");
    if (indirect_result) {
        std::cout << "  Defined indirect destination: LOG -> CSL\n";
    }

    print_separator("4. WRITEQ TD - Writing to Destinations");
    
    // Write to intrapartition queue
    std::vector<String> log_entries = {
        "2025-12-22 10:00:00 System startup initiated",
        "2025-12-22 10:00:01 Loading configuration",
        "2025-12-22 10:00:02 Database connection established",
        "2025-12-22 10:00:03 Ready for transactions"
    };
    
    std::cout << "  Writing to CSL (intrapartition):\n";
    for (const auto& entry : log_entries) {
        auto result = mgr.writeq("CSL", entry);
        if (result) {
            std::cout << "    Written: " << entry.substr(0, 40) << "...\n";
        }
    }
    
    // Write through indirect destination
    std::cout << "\n  Writing through LOG (indirect -> CSL):\n";
    mgr.writeq("LOG", "Indirect log entry via LOG destination");
    std::cout << "    Written via indirect route\n";
    
    // Write to extrapartition (file)
    std::cout << "\n  Writing to PRT (extrapartition/file):\n";
    mgr.writeq("PRT", "=== CICS PRINT OUTPUT ===");
    mgr.writeq("PRT", "Report generated: 2025-12-22");
    mgr.writeq("PRT", "Total transactions: 1,234");
    std::cout << "    Written 3 records to print file\n";

    print_separator("5. ATI Demonstration");
    
    std::cout << "  Writing to MSG to trigger ATI (threshold=3):\n";
    mgr.writeq("MSG", "Message 1 - User login");
    std::cout << "    Record 1 written\n";
    mgr.writeq("MSG", "Message 2 - Transaction started");
    std::cout << "    Record 2 written\n";
    mgr.writeq("MSG", "Message 3 - This should trigger ATI!");
    std::cout << "    Record 3 written\n";

    print_separator("6. READQ TD - Reading from Destinations");
    
    std::cout << "  Reading from CSL:\n";
    int count = 0;
    while (true) {
        auto result = mgr.readq("CSL");
        if (!result) {
            if (result.error().code == ErrorCode::VSAM_END_OF_FILE) {
                std::cout << "    (Queue empty after " << count << " reads)\n";
            }
            break;
        }
        std::cout << "    Read: " << result.value().to_string().substr(0, 50) << "\n";
        ++count;
    }

    print_separator("7. Queue Depth and Statistics");
    
    // Check queue depth for MSG
    auto depth = mgr.get_queue_depth("MSG");
    if (depth) {
        std::cout << "  MSG queue depth: " << depth.value() << " records\n";
    }
    
    std::cout << "\n  Destination listing:\n";
    for (const auto& dest : mgr.list_destinations()) {
        auto type = mgr.get_destination_type(dest);
        String type_str = "UNKNOWN";
        if (type.has_value()) {
            switch (type.value()) {
                case TDQType::INTRAPARTITION: type_str = "INTRA"; break;
                case TDQType::EXTRAPARTITION: type_str = "EXTRA"; break;
                case TDQType::INDIRECT: type_str = "INDIRECT"; break;
                default: break;
            }
        }
        std::cout << "    " << std::setw(8) << dest << " [" << type_str << "]\n";
    }
    
    std::cout << "\n" << mgr.get_statistics() << "\n";

    print_separator("8. Enable/Disable Destinations");
    
    // Disable a destination
    auto disable_result = mgr.disable_destination("MSG");
    if (disable_result) {
        std::cout << "  Disabled destination MSG\n";
        
        // Try to write (should fail)
        auto write_result = mgr.writeq("MSG", "This should fail");
        if (!write_result) {
            std::cout << "  Write to disabled queue failed as expected\n";
        }
        
        // Re-enable
        mgr.enable_destination("MSG");
        std::cout << "  Re-enabled destination MSG\n";
    }

    print_separator("9. DELETEQ TD - Deleting Destinations");
    
    // Delete a destination
    auto del_result = mgr.deleteq("MSG");
    if (del_result) {
        std::cout << "  Deleted destination MSG\n";
    }
    
    std::cout << "\n  Remaining destinations: " << mgr.destination_count() << "\n";

    print_separator("10. Verify Extrapartition Output");
    
    // Read and display the extrapartition file content
    std::ifstream print_file("/tmp/cics_print_output.txt", std::ios::binary);
    if (print_file) {
        std::cout << "  Contents of /tmp/cics_print_output.txt:\n";
        
        // Read variable-length records
        while (print_file) {
            UInt32 len = 0;
            print_file.read(reinterpret_cast<char*>(&len), sizeof(len));
            if (print_file.eof()) break;
            
            String record(len, '\0');
            print_file.read(record.data(), len);
            if (print_file) {
                std::cout << "    \"" << record << "\"\n";
            }
        }
    }

    // Cleanup
    mgr.shutdown();

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " TDQ Example completed successfully!\n";
    std::cout << std::string(60, '=') << "\n\n";

    return 0;
}
