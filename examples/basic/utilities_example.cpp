// =============================================================================
// CICS Emulation - Utilities Example
// Version: 3.4.6
// Demonstrates: Configuration, Performance Monitoring, CLI, Memory Pools
// =============================================================================

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "cics/common/config.hpp"
#include "cics/common/perf.hpp"
#include "cics/common/cli.hpp"
#include "cics/common/memory_pool.hpp"

using namespace cics;

// Sample class for object pooling
struct Transaction {
    int id = 0;
    std::string name;
    double amount = 0.0;
    
    void reset() {
        id = 0;
        name.clear();
        amount = 0.0;
    }
};

void demo_config() {
    std::cout << "\n=== Configuration Management Demo ===\n";
    
    auto& cfg = config::ConfigManager::instance();
    
    // Set some configuration values
    cfg.set("vsam.buffer_size", "8192");
    cfg.set("logging.level", "INFO");
    cfg.set("cics.max_tasks", static_cast<int64_t>(100));
    cfg.set("cics.enable_tracing", true);
    
    // Read configuration
    std::cout << "VSAM Buffer Size: " << cfg.get_int("vsam.buffer_size", 4096) << "\n";
    std::cout << "Log Level: " << cfg.get_string("logging.level", "WARN") << "\n";
    std::cout << "Max Tasks: " << cfg.get_int("cics.max_tasks", 50) << "\n";
    std::cout << "Tracing Enabled: " << (cfg.get_bool("cics.enable_tracing", false) ? "yes" : "no") << "\n";
    
    // Environment variable override (CICS_VSAM_BUFFER_SIZE would override)
    std::cout << "\n(Set CICS_VSAM_BUFFER_SIZE env var to override vsam.buffer_size)\n";
}

void demo_performance() {
    std::cout << "\n=== Performance Monitoring Demo ===\n";
    
    auto& metrics = perf::MetricsCollector::instance();
    metrics.reset();
    
    // Simulate some operations with timing
    for (int i = 0; i < 100; ++i) {
        {
            CICS_TIMED_SCOPE("file_read");
            // Simulate file read (1-5ms)
            std::this_thread::sleep_for(std::chrono::microseconds(1000 + (i % 5) * 1000));
        }
        
        {
            CICS_TIMED_SCOPE("process_record");
            // Simulate processing (0.5-2ms)
            std::this_thread::sleep_for(std::chrono::microseconds(500 + (i % 4) * 500));
        }
        
        metrics.increment("records_processed");
    }
    
    metrics.gauge("active_connections", 42.0);
    
    // Print statistics
    std::cout << "\nFile Read Stats:\n";
    auto read_stats = metrics.get_stats("file_read");
    std::cout << "  Count: " << read_stats.count << "\n";
    std::cout << "  Mean: " << read_stats.mean << " ms\n";
    std::cout << "  P50: " << read_stats.p50 << " ms\n";
    std::cout << "  P99: " << read_stats.p99 << " ms\n";
    
    std::cout << "\nProcess Record Stats:\n";
    auto proc_stats = metrics.get_stats("process_record");
    std::cout << "  Count: " << proc_stats.count << "\n";
    std::cout << "  Mean: " << proc_stats.mean << " ms\n";
    
    std::cout << "\nCounters:\n";
    std::cout << "  Records Processed: " << metrics.get_counter("records_processed") << "\n";
    
    std::cout << "\nGauges:\n";
    std::cout << "  Active Connections: " << metrics.get_gauge("active_connections") << "\n";
}

void demo_memory_pool() {
    std::cout << "\n=== Memory Pool Demo ===\n";
    
    // Create an object pool with 10 pre-allocated objects
    memory::ObjectPool<Transaction> pool(10, 100);
    
    std::cout << "Initial pool state:\n";
    std::cout << "  Available: " << pool.available() << "\n";
    std::cout << "  Active: " << pool.active() << "\n";
    
    // Acquire objects from the pool
    std::vector<std::shared_ptr<Transaction>> active_txns;
    
    for (int i = 0; i < 5; ++i) {
        auto txn = pool.acquire();
        txn->id = i + 1;
        txn->name = "TXN" + std::to_string(i + 1);
        txn->amount = (i + 1) * 100.0;
        active_txns.push_back(txn);
    }
    
    std::cout << "\nAfter acquiring 5 objects:\n";
    std::cout << "  Available: " << pool.available() << "\n";
    std::cout << "  Active: " << pool.active() << "\n";
    
    // Release some objects (they go back to pool automatically)
    active_txns.pop_back();
    active_txns.pop_back();
    
    std::cout << "\nAfter releasing 2 objects:\n";
    std::cout << "  Available: " << pool.available() << "\n";
    std::cout << "  Active: " << pool.active() << "\n";
    
    // Clear all active transactions
    active_txns.clear();
    
    std::cout << "\nAfter releasing all:\n";
    std::cout << "  Available: " << pool.available() << "\n";
    std::cout << "  Active: " << pool.active() << "\n";
    std::cout << "  Total Created: " << pool.total_created() << "\n";
}

void demo_cli(int argc, char* argv[]) {
    std::cout << "\n=== Command Line Parser Demo ===\n";
    
    cli::ArgParser parser("utilities-example", 
        "CICS Emulation - Utilities Demonstration");
    
    parser
        .add_option("config", 'c', "Configuration file path", "cics.conf")
        .add_option("threads", 't', "Number of worker threads", "4")
        .add_flag("verbose", 'v', "Enable verbose output")
        .add_flag("debug", 'd', "Enable debug mode")
        .add_positional("input", "Input file to process");
    
    // For demo purposes, show what would happen with sample args
    std::cout << "Sample usage:\n";
    parser.show_help();
    
    // Parse actual command line if provided
    if (argc > 1) {
        std::cout << "\nParsing provided arguments...\n";
        if (parser.parse(argc, argv)) {
            std::cout << "Config file: " << parser.get("config", "default.conf") << "\n";
            std::cout << "Threads: " << parser.get("threads", "4") << "\n";
            std::cout << "Verbose: " << (parser.flag("verbose") ? "yes" : "no") << "\n";
            std::cout << "Debug: " << (parser.flag("debug") ? "yes" : "no") << "\n";
            if (auto input = parser.positional(0)) {
                std::cout << "Input file: " << *input << "\n";
            }
        } else {
            std::cout << "Parse error: " << parser.error() << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << R"(
================================================================================
                 CICS Emulation v3.4.6
                     Utilities Demonstration
================================================================================
)";

    demo_config();
    demo_performance();
    demo_memory_pool();
    demo_cli(argc, argv);

    std::cout << "\n=== Demo Complete ===\n";
    return 0;
}
