// =============================================================================
// CICS Emulation - Dump Utilities Example
// Version: 3.4.6
// =============================================================================
// Demonstrates hex dump, storage dump, and comparison utilities
// =============================================================================

#include <cics/dump/dump.hpp>
#include <iostream>
#include <cstring>

using namespace cics;
using namespace cics::dump;

void print_header(const char* title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

int main() {
    std::cout << R"(
+==============================================================+
|       CICS Emulation - Dump Utilities Example     |
|                        Version 3.4.6                         |
+==============================================================+
)";

    // Create sample data
    ByteBuffer sample_data(128);
    for (size_t i = 0; i < sample_data.size(); ++i) {
        sample_data[i] = static_cast<Byte>(i);
    }
    
    // Add some text
    const char* text = "CICS Emulation!";
    std::memcpy(sample_data.data() + 32, text, strlen(text));

    // =========================================================================
    // 1. Basic Hex Dump
    // =========================================================================
    print_header("1. Basic Hex Dump");
    
    std::cout << hex_dump(sample_data.data(), 64);

    // =========================================================================
    // 2. Hex Dump with Custom Options
    // =========================================================================
    print_header("2. Hex Dump with Custom Options");
    
    DumpOptions options;
    options.bytes_per_line = 32;
    options.uppercase_hex = false;
    options.group_bytes = true;
    
    std::cout << "  32 bytes per line, lowercase:\n";
    std::cout << hex_dump(sample_data.data(), 64, options);

    // =========================================================================
    // 3. Storage Dump (CICS-style)
    // =========================================================================
    print_header("3. Storage Dump (CICS-style)");
    
    StorageDumpHeader header;
    header.title = "TRANSACTION ABEND DUMP";
    header.timestamp = "2025-12-22 10:30:45";
    header.transaction_id = "TRNA";
    header.task_number = "00012345";
    header.program_name = "CUSTINQ";
    header.address = 0x00ABC000;
    header.length = 64;
    
    std::cout << storage_dump(sample_data.data(), 64, header);

    // =========================================================================
    // 4. Byte/Hex Conversion Utilities
    // =========================================================================
    print_header("4. Byte/Hex Conversion Utilities");
    
    // Byte to hex
    std::cout << "  byte_to_hex(0xAB): " << byte_to_hex(0xAB) << "\n";
    std::cout << "  byte_to_hex(0xAB, false): " << byte_to_hex(0xAB, false) << "\n";
    
    // Bytes to hex string
    Byte bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
    std::cout << "  bytes_to_hex({DE AD BE EF}): " << bytes_to_hex(bytes, 4) << "\n";
    
    // Hex to bytes
    auto hex_result = hex_to_bytes("CAFEBABE");
    if (hex_result.is_success()) {
        std::cout << "  hex_to_bytes('CAFEBABE'): ";
        for (Byte b : hex_result.value()) {
            std::cout << std::hex << static_cast<int>(b) << " ";
        }
        std::cout << std::dec << "\n";
    }

    // =========================================================================
    // 5. Comparison Dump
    // =========================================================================
    print_header("5. Comparison Dump");
    
    ByteBuffer data1 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    ByteBuffer data2 = {0x01, 0x02, 0xFF, 0x04, 0x05, 0xEE, 0x07, 0x08};
    
    std::cout << compare_dump(data1, data2);

    // =========================================================================
    // 6. Record Dump
    // =========================================================================
    print_header("6. Record Dump");
    
    // Create sample records
    ByteBuffer records(80 * 3);  // 3 records of 80 bytes
    std::memset(records.data(), ' ', records.size());
    std::memcpy(records.data(), "RECORD 001 - CUSTOMER DATA", 26);
    std::memcpy(records.data() + 80, "RECORD 002 - ORDER INFORMATION", 30);
    std::memcpy(records.data() + 160, "RECORD 003 - PAYMENT DETAILS", 28);
    
    std::cout << record_dump(records.data(), 240, 80);

    // =========================================================================
    // 7. Field Dump
    // =========================================================================
    print_header("7. Field Dump");
    
    // Create a structure-like buffer
    ByteBuffer record(100);
    std::memset(record.data(), 0, 100);
    std::memcpy(record.data(), "CUST001", 7);           // Customer ID at 0
    std::memcpy(record.data() + 8, "John Smith", 10);   // Name at 8
    record[32] = 0x00; record[33] = 0x01; record[34] = 0x23; record[35] = 0x4C; // Packed amount
    record[40] = 0x00; record[41] = 0x00; record[42] = 0x07; record[43] = 0xD0; // Binary count (2000)
    
    std::vector<FieldInfo> fields = {
        {"CUST-ID", 0, 7, "CHAR"},
        {"CUST-NAME", 8, 20, "CHAR"},
        {"AMOUNT", 32, 4, "PACKED"},
        {"COUNT", 40, 4, "BINARY"},
        {"FILLER", 44, 16, "HEX"}
    };
    
    std::cout << field_dump(record.data(), 60, fields);

    // =========================================================================
    // 8. Dump Statistics
    // =========================================================================
    print_header("8. Dump Statistics");
    
    DumpStats stats;
    stats.analyze(sample_data.data(), static_cast<UInt32>(sample_data.size()));
    std::cout << stats.to_string();

    // =========================================================================
    // 9. Dump Browser
    // =========================================================================
    print_header("9. Dump Browser");
    
    DumpBrowser browser(sample_data.data(), static_cast<UInt32>(sample_data.size()));
    browser.set_page_size(32);
    
    std::cout << "  Page 1 (offset 0):\n";
    std::cout << browser.current_page();
    
    browser.next_page();
    std::cout << "\n  Page 2 (offset 32):\n";
    std::cout << browser.current_page();
    
    // Search for pattern
    auto search_result = browser.find_text("CICS");
    if (search_result.is_success()) {
        std::cout << "\n  Found 'CICS' at offset: " << search_result.value() << "\n";
    }

    // =========================================================================
    // 10. Printable Character Detection
    // =========================================================================
    print_header("10. Character Detection");
    
    std::cout << "  is_printable_ascii:\n";
    std::cout << "    'A' (0x41): " << is_printable_ascii(0x41) << "\n";
    std::cout << "    '\\n' (0x0A): " << is_printable_ascii(0x0A) << "\n";
    std::cout << "    0x80: " << is_printable_ascii(0x80) << "\n";
    
    std::cout << "\n  get_printable_char:\n";
    std::cout << "    'A' (0x41): '" << get_printable_char(0x41) << "'\n";
    std::cout << "    0x00: '" << get_printable_char(0x00) << "'\n";
    std::cout << "    0xFF: '" << get_printable_char(0xFF) << "'\n";

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " Dump Utilities Example Complete\n";
    std::cout << std::string(60, '=') << "\n";

    return 0;
}
