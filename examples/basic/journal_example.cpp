// =============================================================================
// CICS Emulation - Journal Control Example
// =============================================================================

#include <cics/journal/journal.hpp>
#include <iostream>

using namespace cics;
using namespace cics::journal;

int main() {
    std::cout << "\n";
    std::cout << "+==============================================================+\n";
    std::cout << "|       CICS Emulation - Journal Control Example    |\n";
    std::cout << "|                        Version 3.4.6                         |\n";
    std::cout << "+==============================================================+\n\n";
    
    JournalManager::instance().initialize();
    JournalManager::instance().set_current_transaction("JRNL");
    JournalManager::instance().set_current_task(12345);
    
    // 1. Write to system log
    std::cout << "1. Writing to System Log (DFHLOG)\n";
    std::cout << "---------------------------------------------------------------\n";
    
    exec_cics_log("Application started successfully");
    exec_cics_log("INFO", "User authentication completed");
    exec_cics_log("AUDIT", "Transaction JRNL initiated by user ADMIN");
    
    std::cout << "   Wrote 3 entries to DFHLOG\n\n";
    
    // 2. Write to named journal
    std::cout << "2. Writing to Named Journal (AUDITLOG)\n";
    std::cout << "---------------------------------------------------------------\n";
    
    String audit_data = "CUSTOMER=12345|ACTION=UPDATE|FIELD=ADDRESS|OLD=123 Main|NEW=456 Oak";
    exec_cics_write_journalname("AUDITLOG", "CUSTUPD", audit_data.data(), 
                                 static_cast<UInt32>(audit_data.size()));
    
    std::cout << "   Wrote customer update audit record\n";
    
    String order_data = "ORDER=ORD-001234|AMOUNT=1500.00|STATUS=APPROVED";
    exec_cics_write_journalname("AUDITLOG", "ORDPROC", order_data.data(),
                                 static_cast<UInt32>(order_data.size()));
    
    std::cout << "   Wrote order processing audit record\n\n";
    
    // 3. Write binary data
    std::cout << "3. Writing Binary Data\n";
    std::cout << "---------------------------------------------------------------\n";
    
    ByteBuffer binary_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    exec_cics_write_journalname("BINLOG", binary_data);
    
    std::cout << "   Wrote 16 bytes of binary data to BINLOG\n\n";
    
    // 4. List journals
    std::cout << "4. Active Journals\n";
    std::cout << "---------------------------------------------------------------\n";
    
    auto journals = JournalManager::instance().list_journal_info();
    for (const auto& j : journals) {
        std::cout << "   " << j.name << ": " << j.records_written << " records, "
                  << j.bytes_written << " bytes\n";
    }
    std::cout << "\n";
    
    // 5. Journal statistics
    std::cout << "5. Journal Statistics\n";
    std::cout << "---------------------------------------------------------------\n";
    
    auto stats = JournalManager::instance().get_stats();
    std::cout << "   Journals opened:  " << stats.journals_opened << "\n";
    std::cout << "   Records written:  " << stats.records_written << "\n";
    std::cout << "   Bytes written:    " << stats.bytes_written << "\n\n";
    
    JournalManager::instance().shutdown();
    
    std::cout << "================================================================\n";
    std::cout << "   Journal Control example completed successfully!\n";
    std::cout << "   Check /tmp/cics_journals for journal files.\n";
    std::cout << "================================================================\n\n";
    
    return 0;
}
