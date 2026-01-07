// =============================================================================
// CICS Emulation - Basic CICS Example
// =============================================================================

#include <iostream>
#include "cics/common/types.hpp"
#include "cics/cics/cics_types.hpp"

namespace cc = cics::cics;
using cics::FixedString;
using cics::Milliseconds;

int main() {
    std::cout << "CICS Emulation - Basic CICS Example\n";
    std::cout << "================================================\n\n";
    
    // Step 1: Create and initialize EIB
    std::cout << "Step 1: Create EIB\n";
    cc::EIB eib;
    eib.reset();
    eib.set_time_date();
    eib.eibtrnid = FixedString<4>("TEST");
    std::cout << "  Transaction ID: " << eib.eibtrnid.trimmed() << "\n";
    std::cout << "  Time: " << eib.eibtime << "\n";
    std::cout << "  Date: " << eib.eibdate << "\n\n";
    
    // Step 2: Create COMMAREA with input data
    std::cout << "Step 2: Create COMMAREA\n";
    cc::Commarea commarea(256);
    commarea.resize(100);
    commarea.set_string(0, "INPUT-REQUEST", 20);
    commarea.set_value<uint32_t>(20, 12345);
    std::cout << "  Length: " << commarea.length() << " bytes\n";
    std::cout << "  Input: " << commarea.get_string(0, 13) << "\n";
    std::cout << "  Value at 20: " << commarea.get_value<uint32_t>(20) << "\n\n";
    
    // Step 3: Define transaction
    std::cout << "Step 3: Define Transaction\n";
    cc::TransactionDefinition txn("TEST", "TESTPGM");
    txn.priority = 100;
    std::cout << "  ID: " << txn.transaction_id.trimmed() << "\n";
    std::cout << "  Program: " << txn.program_name.trimmed() << "\n";
    std::cout << "  Priority: " << txn.priority << "\n\n";
    
    // Step 4: Create task
    std::cout << "Step 4: Create Task\n";
    cc::CicsTask task(1001, "TEST", "TRM1");
    std::cout << "  Task Number: " << task.task_number() << "\n";
    std::cout << "  Transaction ID: " << task.transaction_id().trimmed() << "\n";
    
    // Step 5: Simulate processing
    std::cout << "\nStep 5: Simulate Processing\n";
    task.set_status(cc::TransactionStatus::RUNNING);
    std::cout << "  Status: RUNNING\n";
    
    // Write output to commarea
    commarea.set_string(50, "OUTPUT-RESPONSE", 20);
    commarea.set_value<uint32_t>(70, 54321);
    
    task.set_status(cc::TransactionStatus::COMPLETED);
    std::cout << "  Status: COMPLETED\n";
    std::cout << "  Output: " << commarea.get_string(50, 15) << "\n";
    std::cout << "  Result: " << commarea.get_value<uint32_t>(70) << "\n\n";
    
    // Step 6: Record statistics
    std::cout << "Step 6: Statistics\n";
    cc::CicsStatistics stats;
    stats.record_transaction(Milliseconds(45), true, false);
    std::cout << "  " << stats.to_string() << "\n";
    
    std::cout << "\nCICS Example completed successfully!\n";
    return 0;
}
