#include "../framework/test_framework.hpp"
#include "cics/vsam/vsam_types.hpp"

using namespace cics;
using namespace cics::vsam;
using namespace cics::test;

void test_vsam_key() {
    VsamKey key1("KEY001");
    ASSERT_EQ(key1.length(), 6u);
    ASSERT_FALSE(key1.empty());
    
    VsamKey key2("KEY002");
    ASSERT_TRUE(key1 < key2);
    
    VsamKey prefix("KEY");
    ASSERT_TRUE(key1.starts_with(prefix));
    
    ASSERT_EQ(key1.to_hex(), "4B4559303031");
}

void test_vsam_address() {
    VsamAddress addr1;
    addr1.rba = 0x1000;
    ASSERT_TRUE(addr1.has_rba());
    ASSERT_FALSE(addr1.has_rrn());
    
    VsamAddress addr2;
    addr2.rrn = 100;
    ASSERT_TRUE(addr2.has_rrn());
}

void test_vsam_record() {
    ByteBuffer data = {0x01, 0x02, 0x03, 0x04};
    VsamKey key("REC001");
    VsamRecord rec(key, ConstByteSpan(data.data(), data.size()));
    
    ASSERT_EQ(rec.key().length(), 6u);
    ASSERT_EQ(rec.length(), 4u);
    ASSERT_FALSE(rec.is_deleted());
    
    ByteBuffer serialized = rec.serialize();
    ASSERT_GT(serialized.size(), 0u);
}

void test_vsam_definition() {
    VsamDefinition def;
    def.cluster_name = "TEST.CLUSTER";
    def.type = VsamType::KSDS;
    def.key_length = 8;
    def.key_offset = 0;
    def.ci_size = 4096;
    def.average_record_length = 100;
    def.maximum_record_length = 200;
    
    auto result = def.validate();
    ASSERT_TRUE(result.is_success());
}

void test_vsam_definition_invalid() {
    VsamDefinition def;
    def.cluster_name = "";  // Invalid
    def.type = VsamType::KSDS;
    
    auto result = def.validate();
    ASSERT_TRUE(result.is_error());
}

void test_control_interval() {
    ControlInterval ci(1, 4096);
    
    ASSERT_EQ(ci.ci_number, 1u);
    ASSERT_EQ(ci.ci_size, 4096u);
    ASSERT_EQ(ci.free_space, 4096u);
    ASSERT_TRUE(ci.has_space_for(100));
    ASSERT_LT(ci.utilization(), 1.0);
}

void test_vsam_statistics() {
    VsamStatistics stats;
    
    stats.record_write(Milliseconds(5), 100);
    ASSERT_EQ(stats.writes.get(), 1u);
    ASSERT_EQ(stats.inserts.get(), 1u);
    
    stats.record_read(Milliseconds(2));
    ASSERT_EQ(stats.reads.get(), 1u);
    
    stats.record_delete();
    ASSERT_EQ(stats.deletes.get(), 1u);
}

void test_browse_context() {
    BrowseContext ctx;
    
    ASSERT_FALSE(ctx.id().empty());
    ASSERT_TRUE(ctx.at_start());
    ASSERT_FALSE(ctx.at_end());
    
    VsamKey key("TEST");
    VsamAddress addr; addr.rba = 100;
    ctx.set_current(key, addr);
    
    ASSERT_FALSE(ctx.at_start());
    ctx.increment_records();
    ASSERT_EQ(ctx.records_read(), 1u);
}

int main() {
    TestSuite suite("VSAM Tests");
    
    suite.add_test("VsamKey", test_vsam_key);
    suite.add_test("VsamAddress", test_vsam_address);
    suite.add_test("VsamRecord", test_vsam_record);
    suite.add_test("VsamDefinition Valid", test_vsam_definition);
    suite.add_test("VsamDefinition Invalid", test_vsam_definition_invalid);
    suite.add_test("ControlInterval", test_control_interval);
    suite.add_test("VsamStatistics", test_vsam_statistics);
    suite.add_test("BrowseContext", test_browse_context);
    
    TestRunner runner;
    runner.add_suite(&suite);
    return runner.run_all();
}
