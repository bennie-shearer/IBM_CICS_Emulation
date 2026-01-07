// =============================================================================
// CICS Emulation - Spool Control Example
// =============================================================================
// Demonstrates SPOOLOPEN, SPOOLWRITE, SPOOLREAD, SPOOLCLOSE functionality.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#include <cics/spool/spool.hpp>
#include <iostream>
#include <iomanip>

using namespace cics;
using namespace cics::spool;

void print_header(const char* title) {
    std::cout << "\n============================================================\n";
    std::cout << " " << title << "\n";
    std::cout << "============================================================\n";
}

int main() {
    std::cout << "\n";
    std::cout << "+==============================================================+\n";
    std::cout << "|       CICS Emulation - Spool Control Example      |\n";
    std::cout << "|                        Version 3.4.6                         |\n";
    std::cout << "+==============================================================+\n";

    // Initialize
    SpoolManager::instance().initialize();
    SpoolManager::instance().set_spool_directory("/tmp/cics_spool_demo");

    // =========================================================================
    // 1. Basic Spool Output
    // =========================================================================
    print_header("1. Basic Spool Output");
    
    auto open_result = exec_cics_spoolopen_output("REPORT01");
    if (open_result.is_success()) {
        String token = open_result.value();
        std::cout << "  Opened spool file: REPORT01\n";
        std::cout << "  Token: " << token << "\n";
        
        // Write header
        exec_cics_spoolwrite_line(token, "===========================================================");
        exec_cics_spoolwrite_line(token, "                    CUSTOMER REPORT                        ");
        exec_cics_spoolwrite_line(token, "===========================================================");
        exec_cics_spoolwrite_line(token, "");
        exec_cics_spoolwrite_line(token, "Customer ID    Name                    Balance");
        exec_cics_spoolwrite_line(token, "-----------    --------------------    -----------");
        
        // Write data
        exec_cics_spoolwrite_line(token, "CUST001        John Smith              $1,234.56");
        exec_cics_spoolwrite_line(token, "CUST002        Jane Doe                $5,678.90");
        exec_cics_spoolwrite_line(token, "CUST003        Bob Johnson             $9,012.34");
        
        // Write footer
        exec_cics_spoolwrite_line(token, "");
        exec_cics_spoolwrite_line(token, "-----------------------------------------------------------");
        exec_cics_spoolwrite_line(token, "Total Records: 3");
        
        // Get info before closing
        auto info_result = SpoolManager::instance().get_info(token);
        if (info_result.is_success()) {
            std::cout << "  Records written: " << info_result.value().record_count << "\n";
            std::cout << "  Bytes written: " << info_result.value().byte_count << "\n";
        }
        
        // Close the spool
        exec_cics_spoolclose(token);
        std::cout << "  Spool file closed\n";
    }

    // =========================================================================
    // 2. Spool with Class
    // =========================================================================
    print_header("2. Spool with Class");
    
    open_result = exec_cics_spoolopen_output("PRINTOUT", SpoolClass::P);
    if (open_result.is_success()) {
        String token = open_result.value();
        std::cout << "  Opened spool file: PRINTOUT (Class P)\n";
        
        exec_cics_spoolwrite_line(token, "This report goes to the print queue (Class P)");
        exec_cics_spoolwrite_line(token, "It will be held for printing");
        
        exec_cics_spoolclose(token, SpoolDisposition::HOLD);
        std::cout << "  Spool file closed with HOLD disposition\n";
    }

    // =========================================================================
    // 3. Advanced Spool Attributes
    // =========================================================================
    print_header("3. Advanced Spool Attributes");
    
    SpoolAttributes attrs;
    attrs.name = "INVOICE";
    attrs.type = SpoolType::OUTPUT;
    attrs.spool_class = SpoolClass::A;
    attrs.disposition = SpoolDisposition::KEEP;
    attrs.copies = 2;
    attrs.line_numbers = true;
    attrs.page_numbers = true;
    attrs.lines_per_page = 60;
    
    open_result = exec_cics_spoolopen_output(attrs);
    if (open_result.is_success()) {
        String token = open_result.value();
        std::cout << "  Opened spool file: INVOICE\n";
        std::cout << "  Copies: " << attrs.copies << "\n";
        std::cout << "  Line numbers: enabled\n";
        std::cout << "  Page numbers: enabled\n";
        
        for (int i = 1; i <= 5; i++) {
            std::ostringstream line;
            line << "Invoice line " << i << " - Amount: $" << (i * 100) << ".00";
            exec_cics_spoolwrite_line(token, line.str());
        }
        
        exec_cics_spoolclose(token);
        std::cout << "  Spool file closed\n";
    }

    // =========================================================================
    // 4. List Active Spools
    // =========================================================================
    print_header("4. Active Spools");
    
    auto spools = SpoolManager::instance().list_spools();
    std::cout << "  Active spool files: " << spools.size() << "\n";
    for (const auto& info : spools) {
        std::cout << "    - " << info.name << " (" << info.token << ")\n";
    }

    // =========================================================================
    // 5. Statistics
    // =========================================================================
    print_header("5. Spool Statistics");
    
    auto stats = SpoolManager::instance().get_stats();
    std::cout << "  Files Opened:     " << stats.files_opened << "\n";
    std::cout << "  Files Closed:     " << stats.files_closed << "\n";
    std::cout << "  Records Written:  " << stats.records_written << "\n";
    std::cout << "  Records Read:     " << stats.records_read << "\n";
    std::cout << "  Bytes Written:    " << stats.bytes_written << "\n";
    std::cout << "  Bytes Read:       " << stats.bytes_read << "\n";
    std::cout << "  Pages Output:     " << stats.pages_output << "\n";

    // Cleanup
    SpoolManager::instance().shutdown();

    std::cout << "\n================================================================\n";
    std::cout << "Spool Control example completed successfully!\n";
    std::cout << "================================================================\n\n";

    return 0;
}
