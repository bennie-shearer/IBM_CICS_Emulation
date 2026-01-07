#include "../framework/test_framework.hpp"
#include "cics/vsam/vsam_types.hpp"
#include <chrono>
#include <random>
#include <iomanip>

using namespace cics;
using namespace cics::vsam;

class Benchmark {
public:
    static void run(const std::string& name, int iterations, std::function<void()> fn) {
        // Warmup
        for (int i = 0; i < std::min(10, iterations / 10); i++) fn();
        
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) fn();
        auto end = std::chrono::high_resolution_clock::now();
        
        double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double per_op_us = (total_ms * 1000.0) / iterations;
        double ops_per_sec = iterations / (total_ms / 1000.0);
        
        std::cout << std::setw(40) << std::left << name 
                  << std::setw(12) << std::right << std::fixed << std::setprecision(2) << per_op_us << " us/op"
                  << std::setw(15) << std::right << std::fixed << std::setprecision(0) << ops_per_sec << " ops/s\n";
    }
};

void benchmark_vsam_key_creation() {
    Benchmark::run("VsamKey creation", 100000, []() {
        VsamKey key("TESTKEY1234567890");
        (void)key.length();
    });
}

void benchmark_vsam_key_comparison() {
    VsamKey key1("KEY00001");
    VsamKey key2("KEY00002");
    
    Benchmark::run("VsamKey comparison", 1000000, [&]() {
        volatile bool result = key1 < key2;
        (void)result;
    });
}

void benchmark_vsam_record_serialization() {
    ByteBuffer data(100, 0x42);
    VsamKey key("TESTKEY1");
    VsamRecord rec(key, ConstByteSpan(data.data(), data.size()));
    
    Benchmark::run("VsamRecord serialize", 100000, [&]() {
        ByteBuffer serialized = rec.serialize();
        (void)serialized.size();
    });
}

void benchmark_ksds_write() {
    VsamDefinition def;
    def.cluster_name = "BENCH.KSDS";
    def.type = VsamType::KSDS;
    def.key_length = 8;
    def.ci_size = 4096;
    
    auto file = create_vsam_file(def, "");
    file->open(AccessMode::IO, ProcessingMode::DYNAMIC);
    
    ByteBuffer data(100, 0x42);
    int counter = 0;
    
    Benchmark::run("KSDS write", 10000, [&]() {
        String key_str = std::format("K{:07d}", counter++);
        VsamKey key(key_str);
        VsamRecord rec(key, ConstByteSpan(data.data(), data.size()));
        file->write(rec);
    });
    
    file->close();
}

void benchmark_ksds_read() {
    VsamDefinition def;
    def.cluster_name = "BENCH.READ.KSDS";
    def.type = VsamType::KSDS;
    def.key_length = 8;
    def.ci_size = 4096;
    
    auto file = create_vsam_file(def, "");
    file->open(AccessMode::IO, ProcessingMode::DYNAMIC);
    
    ByteBuffer data(100, 0x42);
    for (int i = 0; i < 1000; i++) {
        String key_str = std::format("R{:07d}", i);
        VsamKey key(key_str);
        VsamRecord rec(key, ConstByteSpan(data.data(), data.size()));
        file->write(rec);
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999);
    
    Benchmark::run("KSDS read (random)", 10000, [&]() {
        String key_str = std::format("R{:07d}", dis(gen));
        VsamKey key(key_str);
        auto result = file->read(key);
        (void)result;
    });
    
    file->close();
}

void benchmark_hash_functions() {
    ByteBuffer data(1024, 0xAB);
    ConstByteSpan span(data.data(), data.size());
    
    Benchmark::run("CRC32 (1KB)", 100000, [&]() {
        volatile auto hash = crc32(span);
        (void)hash;
    });
    
    Benchmark::run("FNV1a (1KB)", 100000, [&]() {
        volatile auto hash = fnv1a_hash(span);
        (void)hash;
    });
}

int main() {
    std::cout << "\n+======================================================================+\n";
    std::cout << "|               CICS Emulation Benchmarks                    |\n";
    std::cout << "+======================================================================+\n\n";
    
    benchmark_vsam_key_creation();
    benchmark_vsam_key_comparison();
    benchmark_vsam_record_serialization();
    benchmark_ksds_write();
    benchmark_ksds_read();
    benchmark_hash_functions();
    
    std::cout << "\n";
    return 0;
}
