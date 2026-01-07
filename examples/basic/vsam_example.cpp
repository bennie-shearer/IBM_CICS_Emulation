// =============================================================================
// CICS Emulation - Basic VSAM Example
// =============================================================================

#include <iostream>
#include "cics/vsam/vsam_types.hpp"

using namespace cics;
using namespace cics::vsam;

int main() {
    std::cout << "=== VSAM KSDS Basic Example ===\n\n";
    
    // Step 1: Define the VSAM file
    VsamDefinition def;
    def.cluster_name = "EXAMPLE.CUSTOMER.FILE";
    def.type = VsamType::KSDS;
    def.key_length = 10;
    def.key_offset = 0;
    def.ci_size = 4096;
    def.average_record_length = 100;
    def.maximum_record_length = 200;
    
    // Validate definition
    if (def.validate().is_error()) {
        std::cerr << "Invalid VSAM definition\n";
        return 1;
    }
    
    // Step 2: Create and open the file
    auto file = create_vsam_file(def, "");
    if (!file) {
        std::cerr << "Failed to create VSAM file\n";
        return 1;
    }
    
    auto open_result = file->open(AccessMode::IO, ProcessingMode::DYNAMIC);
    if (open_result.is_error()) {
        std::cerr << "Failed to open: " << open_result.error().message << "\n";
        return 1;
    }
    
    // Step 3: Insert records
    std::cout << "Inserting records...\n";
    for (int i = 1; i <= 10; i++) {
        std::string key_str = std::format("CUST{:06d}", i);
        std::string data_str = std::format("Customer {} - John Doe {}", i, i);
        
        VsamKey key(key_str);
        ByteBuffer data(data_str.begin(), data_str.end());
        VsamRecord rec(key, ConstByteSpan(data.data(), data.size()));
        
        auto result = file->write(rec);
        if (result.is_error()) {
            std::cerr << "Write failed for " << key_str << "\n";
        }
    }
    std::cout << "Inserted " << file->record_count() << " records\n\n";
    
    // Step 4: Read a specific record
    std::cout << "Reading record CUST000005...\n";
    VsamKey search("CUST000005");
    auto read_result = file->read(search);
    if (read_result.is_success()) {
        const auto& rec = read_result.value();
        std::string data(reinterpret_cast<const char*>(rec.data()), rec.length());
        std::cout << "Found: " << data << "\n\n";
    }
    
    // Step 5: Browse records
    std::cout << "Browsing all records...\n";
    VsamKey start_key("CUST000001");
    auto browse_id = file->start_browse(start_key, true, false);
    if (browse_id.is_success()) {
        while (true) {
            auto next = file->read_next(browse_id.value());
            if (next.is_error()) break;
            
            std::string key_str(reinterpret_cast<const char*>(next.value().key().data()), 
                               next.value().key().length());
            std::cout << "  Key: " << key_str << "\n";
        }
        file->end_browse(browse_id.value());
    }
    
    // Step 6: Update a record
    std::cout << "\nUpdating CUST000003...\n";
    VsamKey update_key("CUST000003");
    std::string new_data = "Customer 3 - UPDATED DATA";
    ByteBuffer new_buf(new_data.begin(), new_data.end());
    VsamRecord updated_rec(update_key, ConstByteSpan(new_buf.data(), new_buf.size()));
    file->update(updated_rec);
    
    // Step 7: Delete a record
    std::cout << "Deleting CUST000007...\n";
    VsamKey del_key("CUST000007");
    file->erase(del_key);
    std::cout << "Records remaining: " << file->record_count() << "\n\n";
    
    // Step 8: Display statistics
    const auto& stats = file->statistics();
    std::cout << "=== Statistics ===\n";
    std::cout << "  Reads: " << stats.reads.get() << "\n";
    std::cout << "  Writes: " << stats.writes.get() << "\n";
    std::cout << "  Updates: " << stats.updates.get() << "\n";
    std::cout << "  Deletes: " << stats.deletes.get() << "\n";
    
    // Step 9: Close file
    file->close();
    std::cout << "\nFile closed successfully.\n";
    
    return 0;
}
