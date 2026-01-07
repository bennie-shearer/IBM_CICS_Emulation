#include "benchmark_framework.hpp"
#include "cics/common/types.hpp"
#include "cics/common/threading.hpp"
#include "cics/vsam/vsam_types.hpp"

using namespace cics;
using namespace cics::benchmark;

int main() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  CICS Emulation Benchmark Suite\n";
    std::cout << "========================================\n\n";
    
    Benchmark::print_header();
    
    // String operations
    {
        String test_str = "  Hello, World!  ";
        Benchmark b("String trim", 100000);
        Benchmark::print_result(b.run([&]() { (void)trim(test_str); }));
    }
    
    {
        String test_str = "hello";
        Benchmark b("String to_upper", 100000);
        Benchmark::print_result(b.run([&]() { (void)to_upper(test_str); }));
    }
    
    {
        String test_str = "a,b,c,d,e,f,g,h,i,j";
        Benchmark b("String split", 50000);
        Benchmark::print_result(b.run([&]() { (void)split(test_str, ','); }));
    }
    
    // Hash functions
    {
        ByteBuffer data(100, 0x42);
        Benchmark b("CRC32 (100 bytes)", 100000);
        Benchmark::print_result(b.run([&]() { (void)crc32(data); }));
    }
    
    {
        ByteBuffer data(1000, 0x42);
        Benchmark b("CRC32 (1000 bytes)", 50000);
        Benchmark::print_result(b.run([&]() { (void)crc32(data); }));
    }
    
    {
        ByteBuffer data(100, 0x42);
        Benchmark b("FNV1a (100 bytes)", 100000);
        Benchmark::print_result(b.run([&]() { (void)fnv1a_hash(data); }));
    }
    
    // UUID generation
    {
        Benchmark b("UUID generate", 50000);
        Benchmark::print_result(b.run([]() { (void)UUID::generate(); }));
    }
    
    // EBCDIC conversion
    {
        String ascii = "HELLO WORLD FROM CICS";
        Benchmark b("ASCII to EBCDIC", 100000);
        Benchmark::print_result(b.run([&]() { (void)ascii_to_ebcdic(ascii); }));
    }
    
    // Hex conversion
    {
        ByteBuffer data(50, 0xAB);
        Benchmark b("to_hex_string (50 bytes)", 100000);
        Benchmark::print_result(b.run([&]() { (void)to_hex_string(data); }));
    }
    
    // VSAM key operations
    {
        vsam::VsamKey key1("CUSTOMER001");
        vsam::VsamKey key2("CUSTOMER002");
        Benchmark b("VsamKey comparison", 200000);
        Benchmark::print_result(b.run([&]() { volatile bool x = key1 < key2; (void)x; }));
    }
    
    // FixedString operations
    {
        FixedString<8> fs1("TEST");
        FixedString<8> fs2("TEST");
        Benchmark b("FixedString comparison", 200000);
        Benchmark::print_result(b.run([&]() { volatile bool x = fs1 == fs2; (void)x; }));
    }
    
    // Atomic counter
    {
        AtomicCounter<> counter;
        Benchmark b("AtomicCounter increment", 500000);
        Benchmark::print_result(b.run([&]() { counter++; }));
    }
    
    // Thread pool (if available)
    {
        auto& pool = threading::global_thread_pool();
        std::atomic<int> work_done{0};
        Benchmark b("ThreadPool submit", 10000);
        Benchmark::print_result(b.run([&]() { 
            pool.execute([&work_done]() { work_done++; }); 
        }));
        pool.wait_all();
    }
    
    std::cout << "\n========================================\n";
    std::cout << "  Benchmarks complete\n";
    std::cout << "========================================\n\n";
    
    return 0;
}
