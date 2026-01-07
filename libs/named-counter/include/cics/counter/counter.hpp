// =============================================================================
// CICS Emulation - Named Counter
// =============================================================================
// Provides GET COUNTER, PUT COUNTER, UPDATE COUNTER functionality.
// Implements sequence generation and named counter management.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#ifndef CICS_COUNTER_HPP
#define CICS_COUNTER_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

namespace cics {
namespace counter {

// =============================================================================
// Constants
// =============================================================================

constexpr UInt32 MAX_COUNTER_NAME = 16;
constexpr Int64 DEFAULT_MIN_VALUE = 0;
constexpr Int64 DEFAULT_MAX_VALUE = INT64_MAX;
constexpr Int64 DEFAULT_INCREMENT = 1;

// =============================================================================
// Counter Options
// =============================================================================

struct CounterOptions {
    Int64 minimum = DEFAULT_MIN_VALUE;
    Int64 maximum = DEFAULT_MAX_VALUE;
    Int64 increment = DEFAULT_INCREMENT;
    bool wrap = false;              // Wrap around at max
    bool recoverable = false;       // Survives restarts
};

// =============================================================================
// Counter Information
// =============================================================================

struct CounterInfo {
    String name;
    String pool;
    Int64 current_value;
    Int64 minimum;
    Int64 maximum;
    Int64 increment;
    bool wrap;
    bool recoverable;
    UInt64 get_count;
    UInt64 update_count;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_accessed;
};

// =============================================================================
// Named Counter
// =============================================================================

class NamedCounter {
public:
    NamedCounter(StringView name, Int64 initial_value, const CounterOptions& opts);
    
    // Properties
    [[nodiscard]] const String& name() const { return name_; }
    [[nodiscard]] Int64 current() const;
    [[nodiscard]] Int64 minimum() const { return minimum_; }
    [[nodiscard]] Int64 maximum() const { return maximum_; }
    [[nodiscard]] Int64 increment() const { return increment_; }
    [[nodiscard]] bool wraps() const { return wrap_; }
    
    // Operations
    Result<Int64> get();                            // Get and increment
    Result<Int64> get(Int64 increment);             // Get and increment by value
    Result<void> put(Int64 value);                  // Set value
    Result<Int64> update(Int64 expected, Int64 new_value);  // Compare and swap
    Result<void> redefine(const CounterOptions& opts);
    
    // Information
    [[nodiscard]] CounterInfo get_info() const;
    
private:
    String name_;
    std::atomic<Int64> value_;
    Int64 minimum_;
    Int64 maximum_;
    Int64 increment_;
    bool wrap_;
    bool recoverable_;
    std::atomic<UInt64> get_count_{0};
    std::atomic<UInt64> update_count_{0};
    std::chrono::system_clock::time_point created_;
    mutable std::chrono::system_clock::time_point last_accessed_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Counter Pool
// =============================================================================

class CounterPool {
public:
    explicit CounterPool(StringView name);
    
    [[nodiscard]] const String& name() const { return name_; }
    
    // Counter operations
    Result<NamedCounter*> define(StringView counter_name, Int64 initial, 
                                  const CounterOptions& opts = {});
    Result<NamedCounter*> get_counter(StringView name);
    Result<void> delete_counter(StringView name);
    [[nodiscard]] bool exists(StringView name) const;
    
    // Quick access
    Result<Int64> get(StringView name);
    Result<Int64> get(StringView name, Int64 increment);
    Result<void> put(StringView name, Int64 value);
    
    // List operations
    [[nodiscard]] std::vector<String> list_counters() const;
    [[nodiscard]] std::vector<CounterInfo> list_counter_info() const;
    [[nodiscard]] UInt32 count() const;
    
private:
    String name_;
    std::unordered_map<String, std::unique_ptr<NamedCounter>> counters_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Counter Statistics
// =============================================================================

struct CounterStats {
    UInt64 counters_defined{0};
    UInt64 counters_deleted{0};
    UInt64 gets_executed{0};
    UInt64 puts_executed{0};
    UInt64 updates_executed{0};
    UInt64 wrap_arounds{0};
};

// =============================================================================
// Counter Manager
// =============================================================================

class CounterManager {
public:
    static CounterManager& instance();
    
    // Lifecycle
    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }
    
    // Pool management
    Result<CounterPool*> create_pool(StringView name);
    Result<CounterPool*> get_pool(StringView name);
    Result<void> delete_pool(StringView name);
    [[nodiscard]] CounterPool* default_pool();
    
    // Counter operations (on default pool)
    Result<Int64> define_counter(StringView name, Int64 initial, 
                                  const CounterOptions& opts = {});
    Result<Int64> get(StringView name);
    Result<Int64> get(StringView name, Int64 increment);
    Result<void> put(StringView name, Int64 value);
    Result<Int64> update(StringView name, Int64 expected, Int64 new_value);
    Result<void> delete_counter(StringView name);
    
    // Counter operations (with pool)
    Result<Int64> get(StringView pool, StringView name);
    Result<void> put(StringView pool, StringView name, Int64 value);
    
    // Query
    Result<CounterInfo> query(StringView name);
    [[nodiscard]] std::vector<String> list_pools() const;
    
    // Statistics
    [[nodiscard]] CounterStats get_stats() const;
    void reset_stats();
    
private:
    CounterManager() = default;
    ~CounterManager() = default;
    CounterManager(const CounterManager&) = delete;
    CounterManager& operator=(const CounterManager&) = delete;
    
    bool initialized_ = false;
    std::unordered_map<String, std::unique_ptr<CounterPool>> pools_;
    CounterStats stats_;
    mutable std::mutex mutex_;
};

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

// DEFINE COUNTER
Result<Int64> exec_cics_define_counter(StringView name, Int64 initial);
Result<Int64> exec_cics_define_counter(StringView name, Int64 initial, 
                                        const CounterOptions& opts);

// GET COUNTER
Result<Int64> exec_cics_get_counter(StringView name);
Result<Int64> exec_cics_get_counter(StringView name, Int64 increment);
Result<Int64> exec_cics_get_counter(StringView pool, StringView name);

// PUT COUNTER
Result<void> exec_cics_put_counter(StringView name, Int64 value);
Result<void> exec_cics_put_counter(StringView pool, StringView name, Int64 value);

// UPDATE COUNTER (compare and swap)
Result<Int64> exec_cics_update_counter(StringView name, Int64 expected, Int64 new_value);

// DELETE COUNTER
Result<void> exec_cics_delete_counter(StringView name);

// QUERY COUNTER
Result<CounterInfo> exec_cics_query_counter(StringView name);

} // namespace counter
} // namespace cics

#endif // CICS_COUNTER_HPP
