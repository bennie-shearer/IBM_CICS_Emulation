#include "../framework/test_framework.hpp"
#include "cics/vsam/vsam_types.hpp"
#include "cics/catalog/master_catalog.hpp"

using namespace cics;
using namespace cics::vsam;
using namespace cics::catalog;
using namespace cics::test;

void test_ksds_file_operations() {
    VsamDefinition def;
    def.cluster_name = "TEST.KSDS.FILE";
    def.type = VsamType::KSDS;
    def.key_length = 8;
    def.key_offset = 0;
    def.ci_size = 4096;
    def.average_record_length = 100;
    def.maximum_record_length = 200;
    
    auto file = create_vsam_file(def, "");
    ASSERT_TRUE(file != nullptr);
    
    auto open_result = file->open(AccessMode::IO, ProcessingMode::DYNAMIC);
    ASSERT_TRUE(open_result.is_success());
    ASSERT_TRUE(file->is_open());
    
    // Write records
    for (int i = 1; i <= 10; i++) {
        String key_str = std::format("KEY{:05d}", i);
        String data_str = std::format("Data for record {}", i);
        VsamKey key(key_str);
        ByteBuffer data(data_str.begin(), data_str.end());
        VsamRecord rec(key, ConstByteSpan(data.data(), data.size()));
        
        auto write_result = file->write(rec);
        ASSERT_TRUE(write_result.is_success());
    }
    
    ASSERT_EQ(file->record_count(), 10u);
    
    // Read by key
    VsamKey read_key("KEY00005");
    auto read_result = file->read(read_key);
    ASSERT_TRUE(read_result.is_success());
    ASSERT_GT(read_result.value().length(), 0u);
    
    // Update
    ByteBuffer updated_data = {'U', 'P', 'D', 'A', 'T', 'E', 'D'};
    VsamRecord updated(read_key, ConstByteSpan(updated_data.data(), updated_data.size()));
    auto update_result = file->update(updated);
    ASSERT_TRUE(update_result.is_success());
    
    // Delete
    VsamKey del_key("KEY00003");
    auto del_result = file->erase(del_key);
    ASSERT_TRUE(del_result.is_success());
    ASSERT_EQ(file->record_count(), 9u);
    
    auto close_result = file->close();
    ASSERT_TRUE(close_result.is_success());
}

void test_vsam_browse() {
    VsamDefinition def;
    def.cluster_name = "TEST.BROWSE.FILE";
    def.type = VsamType::KSDS;
    def.key_length = 6;
    def.ci_size = 4096;
    
    auto file = create_vsam_file(def, "");
    file->open(AccessMode::IO, ProcessingMode::DYNAMIC);
    
    // Insert test data
    for (int i = 1; i <= 5; i++) {
        String key_str = std::format("REC{:03d}", i);
        ByteBuffer data = {'D', 'A', 'T', 'A'};
        VsamRecord rec(VsamKey(key_str), ConstByteSpan(data.data(), data.size()));
        file->write(rec);
    }
    
    // Start browse
    VsamKey start_key("REC001");
    auto browse_result = file->start_browse(start_key, true, false);
    ASSERT_TRUE(browse_result.is_success());
    String browse_id = browse_result.value();
    
    // Read next
    int count = 0;
    while (true) {
        auto next = file->read_next(browse_id);
        if (next.is_error()) break;
        count++;
    }
    ASSERT_GE(count, 4);  // Should read remaining records
    
    file->end_browse(browse_id);
    file->close();
}

void test_catalog_vsam_integration() {
    auto catalog = MasterCatalogFactory::get_default();
    
    CatalogEntry entry;
    entry.name = "USER.VSAM.KSDS";
    entry.type = EntryType::CLUSTER;
    entry.organization = DatasetOrganization::VSAM_KSDS;
    entry.volume = "VSAM01";
    entry.size_bytes = 1024 * 1024;
    
    auto def_result = catalog->define_dataset(entry);
    ASSERT_TRUE(def_result.is_success());
    
    auto get_result = catalog->get_dataset("USER.VSAM.KSDS");
    ASSERT_TRUE(get_result.is_success());
    ASSERT_EQ(get_result.value().type, EntryType::CLUSTER);
    
    auto del_result = catalog->delete_dataset("USER.VSAM.KSDS");
    ASSERT_TRUE(del_result.is_success());
}

int main() {
    TestSuite suite("VSAM Integration Tests");
    
    suite.add_test("KSDS File Operations", test_ksds_file_operations);
    suite.add_test("VSAM Browse", test_vsam_browse);
    suite.add_test("Catalog-VSAM Integration", test_catalog_vsam_integration);
    
    TestRunner runner;
    runner.add_suite(&suite);
    return runner.run_all();
}
