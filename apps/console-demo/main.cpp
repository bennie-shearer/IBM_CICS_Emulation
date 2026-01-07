// =============================================================================
// CICS Emulation - Console Demo Application
// Version: 3.4.6
// =============================================================================

#include <iostream>
#include <iomanip>
#include <string>

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include "cics/common/threading.hpp"
#include "cics/vsam/vsam_types.hpp"
#include "cics/cics/cics_types.hpp"
#include "cics/catalog/master_catalog.hpp"
#include "cics/gdg/gdg_types.hpp"
#include "cics/dfsmshsm/dfsmshsm_types.hpp"

namespace cc = cics::cics;
namespace cv = cics::vsam;
namespace cat = cics::catalog;
namespace gdg = cics::gdg;
namespace hsm = cics::dfsmshsm;
namespace thr = cics::threading;

void print_header() {
    std::cout << R"(
================================================================================
                    CICS Emulation v3.4.6
                         Console Demonstration
================================================================================
)" << std::endl;
}

void print_section(const std::string& title) {
    std::cout << "\n--- " << title << " ---\n";
}

void demo_types() {
    print_section("Core Types Demonstration");
    
    // FixedString
    cics::FixedString<8> txn_id("DEMO");
    std::cout << "FixedString: '" << txn_id.str() << "' (trimmed: '" << txn_id.trimmed() << "')\n";
    
    // UUID
    auto uuid = cics::UUID::generate();
    std::cout << "Generated UUID: " << uuid.to_string() << "\n";
    
    // Version
    cics::Version ver{3, 1, 1, ""};
    std::cout << "Version: " << ver.to_string() << "\n";
    
    // PackedDecimal
    cics::PackedDecimal pd;
    pd.from_string("12345");
    std::cout << "PackedDecimal: " << pd.to_string() << " (int64: " << pd.to_int64() << ")\n";
    
    // EBCDIC conversion
    auto ebcdic = cics::ascii_to_ebcdic("HELLO");
    auto ascii_back = cics::ebcdic_to_ascii(ebcdic);
    std::cout << "EBCDIC round-trip: 'HELLO' -> '" << ascii_back << "'\n";
    
    // CRC32
    std::string test_data = "Test data for CRC32";
    cics::ByteBuffer buf(test_data.begin(), test_data.end());
    auto crc = cics::crc32(buf);
    std::cout << "CRC32: 0x" << std::hex << crc << std::dec << "\n";
}

void demo_error_handling() {
    print_section("Error Handling Demonstration");
    
    // Result success
    cics::Result<int> success_result = cics::make_success(42);
    std::cout << "Success result: " << (success_result.is_success() ? "yes" : "no");
    if (success_result.is_success()) {
        std::cout << ", value = " << success_result.value();
    }
    std::cout << "\n";
    
    // Result error
    cics::Result<int> error_result = cics::make_error<int>(
        cics::ErrorCode::INVALID_ARGUMENT, "Test error message");
    std::cout << "Error result: " << (error_result.is_error() ? "yes" : "no");
    if (error_result.is_error()) {
        std::cout << ", message = " << error_result.error().message;
    }
    std::cout << "\n";
    
    // value_or
    std::cout << "value_or(99): " << error_result.value_or(99) << "\n";
}

void demo_vsam() {
    print_section("VSAM Demonstration");
    
    // Define VSAM file
    cv::VsamDefinition def;
    def.cluster_name = "CUSTOMER.MASTER";
    def.type = cv::VsamType::KSDS;
    def.average_record_length = 256;
    def.key_length = 10;
    def.key_offset = 0;
    
    std::cout << "VSAM Definition created: " << def.cluster_name << "\n";
    std::cout << "  Type: KSDS, Key Length: " << def.key_length << "\n";
    std::cout << "  CI Size: " << def.ci_size << ", Free CI: " << (int)def.free_ci_percent << "%\n";
    
    // Show VSAM key usage
    std::string key_str = "CUST10001";
    cics::ByteBuffer key_buf(key_str.begin(), key_str.end());
    cv::VsamKey key(key_buf);
    std::cout << "  VsamKey hex: " << key.to_hex() << "\n";
    
    // Show record usage
    std::string data_str = "Customer data record";
    cics::ByteBuffer data_buf(data_str.begin(), data_str.end());
    cv::VsamRecord record(key, data_buf);
    std::cout << "  VsamRecord length: " << record.length() << " bytes\n";
}

void demo_cics() {
    print_section("CICS Demonstration");
    
    // EIB
    cc::EIB eib;
    eib.reset();
    eib.set_time_date();
    eib.eibtrnid = cics::FixedString<4>("DEMO");
    std::cout << "EIB: trnid=" << eib.eibtrnid.trimmed() 
              << ", time=" << eib.eibtime 
              << ", date=" << eib.eibdate << "\n";
    
    // Commarea
    cc::Commarea comm(100);
    comm.resize(50);
    comm.set_string(0, "INPUT-DATA", 20);
    std::cout << "COMMAREA: length=" << comm.length() 
              << ", data='" << comm.get_string(0, 10) << "'\n";
    
    // Transaction Definition
    cc::TransactionDefinition txn("DEMO", "DEMOPGM");
    txn.priority = 5;
    std::cout << "Transaction: id=" << txn.transaction_id.trimmed() 
              << ", program=" << txn.program_name.trimmed()
              << ", priority=" << txn.priority << "\n";
    
    // Task
    cc::CicsTask task(1001, "DEMO", "TRM1");
    task.set_status(cc::TransactionStatus::RUNNING);
    std::cout << "Task: number=" << task.task_number() 
              << ", status=RUNNING\n";
    
    // Statistics
    cc::CicsStatistics stats;
    stats.record_transaction(cics::Milliseconds(150), true, false);
    stats.record_transaction(cics::Milliseconds(200), true, false);
    stats.record_transaction(cics::Milliseconds(50), false, true);
    std::cout << "CICS Stats: " << stats.to_string() << "\n";
}

void demo_catalog() {
    print_section("Master Catalog Demonstration");
    
    auto catalog = cat::MasterCatalogFactory::get_default();
    
    // Define datasets
    cat::CatalogEntry entry1;
    entry1.name = "USER.DATA.FILE1";
    entry1.type = cat::EntryType::CLUSTER;
    entry1.organization = cat::DatasetOrganization::VSAM_KSDS;
    entry1.volume = "VOL001";
    catalog->define_dataset(entry1);
    std::cout << "Defined: " << entry1.name << "\n";
    
    cat::CatalogEntry entry2;
    entry2.name = "USER.DATA.FILE2";
    entry2.type = cat::EntryType::CLUSTER;
    entry2.organization = cat::DatasetOrganization::VSAM_ESDS;
    entry2.volume = "VOL001";
    catalog->define_dataset(entry2);
    std::cout << "Defined: " << entry2.name << "\n";
    
    // List with pattern
    auto matches = catalog->list_datasets("USER.DATA.*");
    std::cout << "Pattern 'USER.DATA.*' matches " << matches.size() << " datasets\n";
    
    // Get statistics by reference
    const auto& stats = catalog->statistics();
    std::cout << "Catalog entries: " << stats.total_entries.get() << "\n";
}

void demo_gdg() {
    print_section("GDG Demonstration");
    
    gdg::GdgManager mgr;
    
    // Define GDG base
    gdg::GdgBase base;
    base.name = "USER.GDG.BASE";
    base.limit = 5;
    base.model = gdg::GdgModel::FIFO;
    mgr.define_base(base);
    std::cout << "Defined GDG base: " << base.name << " (limit=" << base.limit << ")\n";
    
    // Create generations
    for (int i = 0; i < 3; ++i) {
        auto gen = mgr.create_generation(base.name);
        if (gen) {
            std::cout << "  Created generation: " << gen->generation_name << "\n";
        }
    }
    
    // List generations
    auto gens = mgr.list_generations(base.name);
    std::cout << "Total generations: " << gens.size() << "\n";
    
    // Get current (relative 0)
    auto current = mgr.get_generation(base.name, 0);
    if (current) {
        std::cout << "Current generation (0): " << current->generation_name << "\n";
    }
}

void demo_hsm() {
    print_section("HSM Demonstration");
    
    hsm::StorageManager mgr;
    
    // Migrate dataset
    mgr.migrate("USER.ARCHIVE.DATA1", hsm::StorageLevel::ML1);
    std::cout << "Migrated USER.ARCHIVE.DATA1 to ML1\n";
    
    mgr.migrate("USER.ARCHIVE.DATA2", hsm::StorageLevel::ML2);
    std::cout << "Migrated USER.ARCHIVE.DATA2 to ML2\n";
    
    // Recall
    mgr.recall("USER.ARCHIVE.DATA1");
    std::cout << "Recalled USER.ARCHIVE.DATA1\n";
    
    // Get statistics by reference
    const auto& stats = mgr.statistics();
    std::cout << "HSM Stats: " << stats.to_string() << "\n";
}

void demo_threading() {
    print_section("Threading Demonstration");
    
    // ConcurrentQueue
    thr::ConcurrentQueue<int> cq;
    for (int i = 1; i <= 5; ++i) {
        cq.push(i * 10);
    }
    std::cout << "Queue size: " << cq.size() << "\n";
    
    int val;
    while (cq.try_pop(val)) {
        std::cout << "  Popped: " << val << "\n";
    }
    
    // ThreadPool
    thr::ThreadPool pool;
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 10; ++i) {
        pool.execute([&counter]() {
            counter++;
        });
    }
    
    pool.wait_all();
    std::cout << "ThreadPool completed " << counter.load() << " tasks\n";
}

int main() {
    print_header();
    
    try {
        demo_types();
        demo_error_handling();
        demo_vsam();
        demo_cics();
        demo_catalog();
        demo_gdg();
        demo_hsm();
        demo_threading();
        
        std::cout << "\n";
        print_section("Demo Complete");
        std::cout << "All demonstrations completed successfully!\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
