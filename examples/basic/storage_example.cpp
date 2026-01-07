// =============================================================================
// CICS Emulation - Storage Control Example
// Version: 3.4.6
// =============================================================================
// Demonstrates GETMAIN, FREEMAIN, and storage management
// =============================================================================

#include <cics/storage/storage_control.hpp>
#include <iostream>
#include <iomanip>
#include <cstring>

using namespace cics;
using namespace cics::storage;

void print_header(const char* title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

int main() {
    std::cout << R"(
+==============================================================+
|     CICS Emulation - Storage Control Example      |
|                        Version 3.4.6                         |
+==============================================================+
)";

    auto& scm = StorageControlManager::instance();

    // =========================================================================
    // 1. Basic GETMAIN
    // =========================================================================
    print_header("1. Basic GETMAIN");
    
    std::cout << "  Allocating 256 bytes...\n";
    auto result1 = exec_cics_getmain(256);
    
    if (result1.is_success()) {
        void* ptr = result1.value();
        std::cout << "  Address: " << ptr << "\n";
        std::cout << "  Size:    " << scm.get_block_size(ptr) << " bytes\n";
        
        // Use the storage
        std::memset(ptr, 'A', 256);
        std::cout << "  Filled with 'A' characters\n";
    }

    // =========================================================================
    // 2. GETMAIN with Initialization
    // =========================================================================
    print_header("2. GETMAIN with Initialization");
    
    // Zero-initialized storage
    std::cout << "  Allocating 128 bytes (zero-initialized)...\n";
    auto result2 = exec_cics_getmain_set(128);
    
    if (result2.is_success()) {
        Byte* ptr = static_cast<Byte*>(result2.value());
        std::cout << "  Address: " << static_cast<void*>(ptr) << "\n";
        
        // Verify initialization
        bool all_zero = true;
        for (int i = 0; i < 128; ++i) {
            if (ptr[i] != 0) {
                all_zero = false;
                break;
            }
        }
        std::cout << "  All zeros: " << (all_zero ? "YES" : "NO") << "\n";
    }

    // Custom initialization
    std::cout << "\n  Allocating 64 bytes (initialized to 0xFF)...\n";
    auto result3 = exec_cics_getmain_initimg(64, 0xFF);
    
    if (result3.is_success()) {
        Byte* ptr = static_cast<Byte*>(result3.value());
        std::cout << "  Address: " << static_cast<void*>(ptr) << "\n";
        std::cout << "  First byte: 0x" << std::hex << std::setw(2) 
                  << std::setfill('0') << static_cast<int>(ptr[0]) << std::dec << "\n";
    }

    // =========================================================================
    // 3. Storage Classes
    // =========================================================================
    print_header("3. Storage Classes");
    
    std::cout << "  Allocating from different storage classes:\n";
    
    auto user_storage = scm.getmain(100, StorageClass::USER);
    if (user_storage.is_success()) {
        std::cout << "    USER:    " << user_storage.value() << "\n";
    }
    
    auto cicsdsa_storage = scm.getmain(100, StorageClass::CICSDSA);
    if (cicsdsa_storage.is_success()) {
        std::cout << "    CICSDSA: " << cicsdsa_storage.value() << "\n";
    }
    
    auto shared_storage = exec_cics_getmain_shared(100);
    if (shared_storage.is_success()) {
        std::cout << "    SHARED:  " << shared_storage.value() << "\n";
    }

    // =========================================================================
    // 4. StorageGuard RAII
    // =========================================================================
    print_header("4. StorageGuard RAII");
    
    std::cout << "  Creating scoped storage allocation...\n";
    {
        StorageGuard guard(512);
        
        if (guard.valid()) {
            std::cout << "  Address: " << guard.get() << "\n";
            std::cout << "  Size:    " << guard.size() << " bytes\n";
            
            // Use typed access
            int* numbers = guard.as<int>();
            for (int i = 0; i < 10; ++i) {
                numbers[i] = i * i;
            }
            std::cout << "  Wrote 10 integers (squares): ";
            for (int i = 0; i < 10; ++i) {
                std::cout << numbers[i] << " ";
            }
            std::cout << "\n";
        }
        
        std::cout << "  Current allocated: " << scm.current_allocated() << " bytes\n";
        std::cout << "  Leaving scope (auto-free)...\n";
    }
    std::cout << "  After scope - Current allocated: " << scm.current_allocated() << " bytes\n";

    // =========================================================================
    // 5. Block Information
    // =========================================================================
    print_header("5. Block Information");
    
    auto block_ptr = scm.getmain(200, StorageClass::USER, StorageInit::ZERO, false, "TEST-BLOCK");
    
    if (block_ptr.is_success()) {
        void* ptr = block_ptr.value();
        auto info_result = scm.get_block_info(ptr);
        
        if (info_result.is_success()) {
            const auto& info = info_result.value();
            std::cout << "  Block Information:\n";
            std::cout << "    Address:        " << info.address << "\n";
            std::cout << "    Size:           " << info.size << " bytes\n";
            std::cout << "    Requested:      " << info.requested_size << " bytes\n";
            std::cout << "    Storage Class:  " << static_cast<int>(info.storage_class) << "\n";
            std::cout << "    Shared:         " << (info.shared ? "YES" : "NO") << "\n";
            std::cout << "    Tag:            " << info.tag << "\n";
        }
    }

    // =========================================================================
    // 6. FREEMAIN
    // =========================================================================
    print_header("6. FREEMAIN");
    
    std::cout << "  Current allocation count: " << scm.current_allocated() << " bytes\n";
    
    // Free specific blocks
    if (result1.is_success()) {
        std::cout << "  Freeing first block...\n";
        exec_cics_freemain(result1.value());
    }
    
    std::cout << "  After free: " << scm.current_allocated() << " bytes\n";

    // =========================================================================
    // 7. Storage Utilities
    // =========================================================================
    print_header("7. Storage Utilities");
    
    // Allocate two blocks for comparison
    auto buf1_result = scm.getmain(32);
    auto buf2_result = scm.getmain(32);
    
    if (buf1_result.is_success() && buf2_result.is_success()) {
        void* buf1 = buf1_result.value();
        void* buf2 = buf2_result.value();
        
        // Initialize
        storage_init_value(buf1, 32, 'X');
        storage_init_value(buf2, 32, 'X');
        
        // Compare (should be equal)
        std::cout << "  Comparing two buffers initialized with 'X':\n";
        bool equal = storage_equal(buf1, buf2, 32);
        std::cout << "    Equal: " << (equal ? "YES" : "NO") << "\n";
        
        // Modify one
        static_cast<Byte*>(buf2)[10] = 'Y';
        
        // Compare again
        Int32 cmp = storage_compare(buf1, buf2, 32);
        std::cout << "  After modifying buf2[10]:\n";
        std::cout << "    Compare result: " << cmp << " (negative = buf1 < buf2)\n";
        
        // Copy
        storage_copy(buf1, buf2, 32);
        equal = storage_equal(buf1, buf2, 32);
        std::cout << "  After copying buf2 to buf1:\n";
        std::cout << "    Equal: " << (equal ? "YES" : "NO") << "\n";
        
        scm.freemain(buf1);
        scm.freemain(buf2);
    }

    // =========================================================================
    // 8. Allocation Dump
    // =========================================================================
    print_header("8. Allocation Dump");
    std::cout << scm.dump_allocations();

    // =========================================================================
    // 9. Statistics
    // =========================================================================
    print_header("9. Statistics");
    std::cout << scm.get_statistics();

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " Storage Control Example Complete\n";
    std::cout << std::string(60, '=') << "\n";

    return 0;
}
