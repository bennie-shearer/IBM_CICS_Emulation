#include "../framework/test_framework.hpp"
#include "cics/cics/cics_types.hpp"

namespace cc = cics::cics;
using cics::String;
using cics::UInt32;
using cics::Milliseconds;
using cics::FixedString;

void test_eib_basic() {
    cc::EIB eib;
    eib.reset();
    ASSERT_EQ(eib.eibresp, cc::CicsResponse::NORMAL);
    ASSERT_EQ(eib.eibcalen, 0u);
    eib.set_time_date();
    ASSERT_GT(eib.eibtime, 0u);
    ASSERT_GT(eib.eibdate, 0u);
}

void test_commarea() {
    cc::Commarea comm(100);
    ASSERT_EQ(comm.length(), 100u);  // Constructor initializes with size
    ASSERT_EQ(comm.capacity(), 32767u);  // MAX_COMMAREA_LENGTH
    comm.resize(50);
    ASSERT_EQ(comm.length(), 50u);
}

void test_transaction_definition() {
    cc::TransactionDefinition txn("MENU", "MENUPGM");
    ASSERT_EQ(txn.priority, 100);  // Default priority is 100
    ASSERT_TRUE(txn.enabled);
}

void test_program_definition() {
    cc::ProgramDefinition pgm("TESTPGM");
    ASSERT_EQ(pgm.language, cc::ProgramLanguage::CPP);  // Default is CPP
    ASSERT_TRUE(pgm.enabled);
}

void test_cics_task() {
    cc::CicsTask task(1001, "TEST", "TRM1");
    ASSERT_EQ(task.task_number(), 1001u);
    ASSERT_EQ(task.status(), cc::TransactionStatus::ACTIVE);
    task.set_status(cc::TransactionStatus::RUNNING);
    ASSERT_EQ(task.status(), cc::TransactionStatus::RUNNING);
}

void test_cics_statistics() {
    cc::CicsStatistics stats;
    stats.record_transaction(Milliseconds(100), true, false);
    ASSERT_EQ(stats.total_transactions.get(), 1u);
    ASSERT_EQ(stats.successful_transactions.get(), 1u);
}

void test_response_names() {
    auto name = cc::response_name(cc::CicsResponse::NORMAL);
    ASSERT_FALSE(name.empty());
}

void test_command_names() {
    auto name = cc::command_name(cc::CicsCommand::READ);
    ASSERT_FALSE(name.empty());
}

int main() {
    ::cics::test::TestSuite suite("CICS Tests");
    
    suite.add_test("EIB Basic", test_eib_basic);
    suite.add_test("Commarea", test_commarea);
    suite.add_test("TransactionDefinition", test_transaction_definition);
    suite.add_test("ProgramDefinition", test_program_definition);
    suite.add_test("CicsTask", test_cics_task);
    suite.add_test("CicsStatistics", test_cics_statistics);
    suite.add_test("Response Names", test_response_names);
    suite.add_test("Command Names", test_command_names);
    
    ::cics::test::TestRunner runner;
    runner.add_suite(&suite);
    return runner.run_all();
}
