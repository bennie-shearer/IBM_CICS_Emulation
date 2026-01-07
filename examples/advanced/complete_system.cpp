// =============================================================================
// CICS Emulation - Complete System Integration Example
// =============================================================================

#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include "cics/common/logging.hpp"
#include "cics/common/threading.hpp"
#include "cics/security/security_context.hpp"
#include "cics/catalog/master_catalog.hpp"
#include "cics/vsam/vsam_types.hpp"
#include "cics/cics/cics_types.hpp"
#include "cics/gdg/gdg_types.hpp"
#include "cics/dfsmshsm/dfsmshsm_types.hpp"

namespace cv = cics::vsam;
namespace cc = cics::cics;
namespace cat = cics::catalog;
namespace gdg = cics::gdg;
namespace hsm = cics::dfsmshsm;
namespace sec = cics::security;
namespace thr = cics::threading;

// Simulated customer record
struct CustomerRecord {
    char customer_id[10];
    char name[30];
    char address[50];
    double balance;
    uint32_t account_type;
    
    static constexpr size_t SIZE = 98;
    
    cics::ByteBuffer serialize() const {
        cics::ByteBuffer buf(SIZE);
        std::memcpy(buf.data(), customer_id, 10);
        std::memcpy(buf.data() + 10, name, 30);
        std::memcpy(buf.data() + 40, address, 50);
        std::memcpy(buf.data() + 90, &balance, 8);
        return buf;
    }
    
    static CustomerRecord deserialize(const cics::ByteBuffer& buf) {
        CustomerRecord rec{};
        std::memcpy(rec.customer_id, buf.data(), 10);
        std::memcpy(rec.name, buf.data() + 10, 30);
        std::memcpy(rec.address, buf.data() + 40, 50);
        std::memcpy(&rec.balance, buf.data() + 90, 8);
        return rec;
    }
};

// Banking system that integrates all components
class BankingSystem {
private:
    cics::SharedPtr<cat::MasterCatalog> catalog_;
    std::unique_ptr<cv::IVsamFile> customer_file_;
    gdg::GdgManager gdg_manager_;
    hsm::StorageManager hsm_manager_;
    cc::CicsStatistics stats_;
    bool initialized_ = false;
    
public:
    bool initialize() {
        std::cout << "Initializing Banking System...\n";
        
        // Get master catalog
        catalog_ = cat::MasterCatalogFactory::get_default();
        std::cout << "  Master Catalog: OK\n";
        
        // Define customer VSAM file
        cv::VsamDefinition def;
        def.cluster_name = "BANK.CUSTOMER.MASTER";
        def.type = cv::VsamType::KSDS;
        def.key_length = 10;
        def.key_offset = 0;
        def.average_record_length = CustomerRecord::SIZE;
        
        // Register in catalog
        cat::CatalogEntry entry;
        entry.name = def.cluster_name;
        entry.type = cat::EntryType::CLUSTER;
        entry.organization = cat::DatasetOrganization::VSAM_KSDS;
        entry.volume = "SYSVOL";
        catalog_->define_dataset(entry);
        std::cout << "  Customer File Cataloged: OK\n";
        
        // Define GDG for transaction logs
        gdg::GdgBase log_base;
        log_base.name = "BANK.TRANS.LOG";
        log_base.limit = 7;  // Keep 7 days
        log_base.model = gdg::GdgModel::FIFO;
        gdg_manager_.define_base(log_base);
        std::cout << "  Transaction Log GDG: OK\n";
        
        initialized_ = true;
        std::cout << "Banking System initialized successfully.\n\n";
        return true;
    }
    
    void process_transaction(const std::string& txn_id, const std::string& customer_id, 
                            double amount, const std::string& type) {
        auto start = std::chrono::steady_clock::now();
        
        std::cout << "Processing " << type << " transaction " << txn_id 
                  << " for customer " << customer_id << "\n";
        
        // Create EIB for this transaction
        cc::EIB eib;
        eib.reset();
        eib.set_time_date();
        eib.eibtrnid = cics::FixedString<4>(txn_id.substr(0, 4).c_str());
        
        // Create COMMAREA with transaction data
        cc::Commarea comm(256);
        comm.set_string(0, customer_id, 10);
        comm.set_string(10, type, 10);
        // Store amount as cents (integer)
        uint64_t amount_cents = static_cast<uint64_t>(amount * 100);
        comm.set_value<uint64_t>(20, amount_cents);
        
        // Simulate processing time
        std::this_thread::sleep_for(std::chrono::milliseconds(10 + rand() % 50));
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<cics::Milliseconds>(end - start);
        
        // Record statistics
        stats_.record_transaction(duration, true, false);
        
        std::cout << "  Completed in " << duration.count() << "ms\n";
    }
    
    void create_log_generation() {
        auto gen = gdg_manager_.create_generation("BANK.TRANS.LOG");
        if (gen) {
            std::cout << "Created log generation: " << gen->generation_name << "\n";
        }
    }
    
    void archive_old_data() {
        std::cout << "Archiving old data to HSM...\n";
        hsm_manager_.migrate("BANK.ARCHIVE.2024Q1", hsm::StorageLevel::ML1);
        hsm_manager_.migrate("BANK.ARCHIVE.2023", hsm::StorageLevel::ML2);
        std::cout << "  Archive complete.\n";
    }
    
    void print_statistics() {
        std::cout << "\n=== System Statistics ===\n";
        std::cout << "Transactions: " << stats_.to_string() << "\n";
        
        const auto& cat_stats = catalog_->statistics();
        std::cout << "Catalog Entries: " << cat_stats.total_entries.get() << "\n";
        
        auto log_gens = gdg_manager_.list_generations("BANK.TRANS.LOG");
        std::cout << "Log Generations: " << log_gens.size() << "\n";
        
        const auto& hsm_stats = hsm_manager_.statistics();
        std::cout << "HSM: " << hsm_stats.to_string() << "\n";
    }
    
    void shutdown() {
        std::cout << "\nShutting down Banking System...\n";
        initialized_ = false;
        std::cout << "Shutdown complete.\n";
    }
};

int main() {
    std::cout << R"(
================================================================================
        CICS Emulation - Complete System Integration Demo
================================================================================
)" << std::endl;

    BankingSystem bank;
    
    // Initialize
    if (!bank.initialize()) {
        std::cerr << "Failed to initialize banking system!\n";
        return 1;
    }
    
    // Create initial log generation
    bank.create_log_generation();
    
    // Process some transactions
    std::cout << "\n=== Processing Transactions ===\n";
    bank.process_transaction("TXN001", "CUST000001", 100.00, "DEPOSIT");
    bank.process_transaction("TXN002", "CUST000002", 250.50, "DEPOSIT");
    bank.process_transaction("TXN003", "CUST000001", 50.00, "WITHDRAW");
    bank.process_transaction("TXN004", "CUST000003", 1000.00, "TRANSFER");
    bank.process_transaction("TXN005", "CUST000002", 75.25, "WITHDRAW");
    
    // Create another log generation
    bank.create_log_generation();
    
    // Archive old data
    std::cout << "\n=== Archiving ===\n";
    bank.archive_old_data();
    
    // Print statistics
    bank.print_statistics();
    
    // Shutdown
    bank.shutdown();
    
    std::cout << "\nComplete system demo finished successfully!\n";
    return 0;
}
