// =============================================================================
// CICS Emulation - Named Counter Example
// =============================================================================

#include <cics/counter/counter.hpp>
#include <iostream>
#include <iomanip>

using namespace cics;
using namespace cics::counter;

int main() {
    std::cout << "\n";
    std::cout << "+==============================================================+\n";
    std::cout << "|       CICS Emulation - Named Counter Example      |\n";
    std::cout << "|                        Version 3.4.6                         |\n";
    std::cout << "+==============================================================+\n\n";
    
    CounterManager::instance().initialize();
    
    // 1. Define counters
    std::cout << "1. Defining Named Counters\n";
    std::cout << "---------------------------------------------------------------\n";
    
    exec_cics_define_counter("ORDERNUM", 1000);
    exec_cics_define_counter("INVOICEN", 500000);
    
    CounterOptions ticket_opts;
    ticket_opts.minimum = 1;
    ticket_opts.maximum = 99999;
    ticket_opts.wrap = true;
    exec_cics_define_counter("TICKETNO", 1, ticket_opts);
    
    std::cout << "   ORDERNUM: Starting at 1000 (order numbers)\n";
    std::cout << "   INVOICEN: Starting at 500000 (invoice numbers)\n";
    std::cout << "   TICKETNO: Starting at 1, wraps at 99999\n\n";
    
    // 2. Get counter values
    std::cout << "2. Getting Counter Values (GET COUNTER)\n";
    std::cout << "---------------------------------------------------------------\n";
    
    std::cout << "   Generating order numbers:\n";
    for (int i = 0; i < 5; ++i) {
        auto result = exec_cics_get_counter("ORDERNUM");
        if (result.is_success()) {
            std::cout << "      Order #" << result.value() << "\n";
        }
    }
    
    std::cout << "\n   Generating invoice numbers:\n";
    for (int i = 0; i < 3; ++i) {
        auto result = exec_cics_get_counter("INVOICEN");
        if (result.is_success()) {
            std::cout << "      Invoice #" << result.value() << "\n";
        }
    }
    
    // 3. Get with custom increment
    std::cout << "\n3. Custom Increment (GET COUNTER INCREMENT)\n";
    std::cout << "---------------------------------------------------------------\n";
    
    std::cout << "   Getting ticket numbers with increment of 10:\n";
    for (int i = 0; i < 3; ++i) {
        auto result = exec_cics_get_counter("TICKETNO", 10);
        if (result.is_success()) {
            std::cout << "      Ticket #" << std::setfill('0') << std::setw(5) 
                      << result.value() << "\n";
        }
    }
    
    // 4. Put (set) counter value
    std::cout << "\n4. Setting Counter Value (PUT COUNTER)\n";
    std::cout << "---------------------------------------------------------------\n";
    
    exec_cics_put_counter("ORDERNUM", 2000);
    std::cout << "   Set ORDERNUM to 2000\n";
    
    auto result = exec_cics_get_counter("ORDERNUM");
    std::cout << "   Next order number: " << result.value() << "\n";
    
    // 5. Update (compare and swap)
    std::cout << "\n5. Compare and Swap (UPDATE COUNTER)\n";
    std::cout << "---------------------------------------------------------------\n";
    
    auto query = exec_cics_query_counter("ORDERNUM");
    if (query.is_success()) {
        Int64 current = query.value().current_value;
        std::cout << "   Current ORDERNUM value: " << current << "\n";
        
        auto update = exec_cics_update_counter("ORDERNUM", current, 3000);
        if (update.is_success()) {
            std::cout << "   Successfully updated from " << update.value() << " to 3000\n";
        }
        
        // Try update with wrong expected value
        auto failed = exec_cics_update_counter("ORDERNUM", 2000, 4000);
        if (failed.is_error()) {
            std::cout << "   Update with wrong expected value failed (as expected)\n";
        }
    }
    
    // 6. Query counter info
    std::cout << "\n6. Query Counter Information\n";
    std::cout << "---------------------------------------------------------------\n";
    
    auto info = exec_cics_query_counter("TICKETNO");
    if (info.is_success()) {
        auto& i = info.value();
        std::cout << "   Counter: " << i.name << "\n";
        std::cout << "   Current: " << i.current_value << "\n";
        std::cout << "   Range:   " << i.minimum << " - " << i.maximum << "\n";
        std::cout << "   Wraps:   " << (i.wrap ? "Yes" : "No") << "\n";
        std::cout << "   Gets:    " << i.get_count << "\n";
    }
    
    // 7. Statistics
    std::cout << "\n7. Counter Statistics\n";
    std::cout << "---------------------------------------------------------------\n";
    
    auto stats = CounterManager::instance().get_stats();
    std::cout << "   Counters defined:  " << stats.counters_defined << "\n";
    std::cout << "   Gets executed:     " << stats.gets_executed << "\n";
    std::cout << "   Puts executed:     " << stats.puts_executed << "\n";
    std::cout << "   Updates executed:  " << stats.updates_executed << "\n\n";
    
    CounterManager::instance().shutdown();
    
    std::cout << "================================================================\n";
    std::cout << "   Named Counter example completed successfully!\n";
    std::cout << "================================================================\n\n";
    
    return 0;
}
