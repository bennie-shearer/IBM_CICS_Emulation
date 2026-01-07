// =============================================================================
// CICS Emulation - Named Counter Implementation
// =============================================================================

#include <cics/counter/counter.hpp>
#include <algorithm>

namespace cics {
namespace counter {

// =============================================================================
// NamedCounter Implementation
// =============================================================================

NamedCounter::NamedCounter(StringView name, Int64 initial_value, const CounterOptions& opts)
    : name_(name)
    , value_(initial_value)
    , minimum_(opts.minimum)
    , maximum_(opts.maximum)
    , increment_(opts.increment)
    , wrap_(opts.wrap)
    , recoverable_(opts.recoverable)
    , created_(std::chrono::system_clock::now())
    , last_accessed_(created_)
{
}

Int64 NamedCounter::current() const {
    return value_.load();
}

Result<Int64> NamedCounter::get() {
    return get(increment_);
}

Result<Int64> NamedCounter::get(Int64 inc) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Int64 current = value_.load();
    Int64 next = current + inc;
    
    // Check bounds
    if (next > maximum_) {
        if (wrap_) {
            next = minimum_ + (next - maximum_ - 1);
        } else {
            return make_error<Int64>(ErrorCode::OUT_OF_RANGE,
                "Counter would exceed maximum value");
        }
    } else if (next < minimum_) {
        if (wrap_) {
            next = maximum_ - (minimum_ - next - 1);
        } else {
            return make_error<Int64>(ErrorCode::OUT_OF_RANGE,
                "Counter would go below minimum value");
        }
    }
    
    value_.store(next);
    ++get_count_;
    last_accessed_ = std::chrono::system_clock::now();
    
    return make_success(current);  // Return value before increment
}

Result<void> NamedCounter::put(Int64 value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (value < minimum_ || value > maximum_) {
        return make_error<void>(ErrorCode::OUT_OF_RANGE,
            "Value outside counter range");
    }
    
    value_.store(value);
    last_accessed_ = std::chrono::system_clock::now();
    
    return make_success();
}

Result<Int64> NamedCounter::update(Int64 expected, Int64 new_value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (new_value < minimum_ || new_value > maximum_) {
        return make_error<Int64>(ErrorCode::OUT_OF_RANGE,
            "New value outside counter range");
    }
    
    Int64 current = value_.load();
    if (current != expected) {
        return make_error<Int64>(ErrorCode::INVALID_STATE,
            "Counter value has changed");
    }
    
    value_.store(new_value);
    ++update_count_;
    last_accessed_ = std::chrono::system_clock::now();
    
    return make_success(current);
}

Result<void> NamedCounter::redefine(const CounterOptions& opts) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Int64 current = value_.load();
    if (current < opts.minimum || current > opts.maximum) {
        return make_error<void>(ErrorCode::INVREQ,
            "Current value would be outside new range");
    }
    
    minimum_ = opts.minimum;
    maximum_ = opts.maximum;
    increment_ = opts.increment;
    wrap_ = opts.wrap;
    recoverable_ = opts.recoverable;
    
    return make_success();
}

CounterInfo NamedCounter::get_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    CounterInfo info;
    info.name = name_;
    info.current_value = value_.load();
    info.minimum = minimum_;
    info.maximum = maximum_;
    info.increment = increment_;
    info.wrap = wrap_;
    info.recoverable = recoverable_;
    info.get_count = get_count_.load();
    info.update_count = update_count_.load();
    info.created = created_;
    info.last_accessed = last_accessed_;
    return info;
}

// =============================================================================
// CounterPool Implementation
// =============================================================================

CounterPool::CounterPool(StringView name)
    : name_(name)
{
}

Result<NamedCounter*> CounterPool::define(StringView counter_name, Int64 initial,
                                           const CounterOptions& opts) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (counter_name.size() > MAX_COUNTER_NAME) {
        return make_error<NamedCounter*>(ErrorCode::INVREQ,
            "Counter name too long");
    }
    
    String key(counter_name);
    auto it = counters_.find(key);
    if (it != counters_.end()) {
        return make_error<NamedCounter*>(ErrorCode::DUPLICATE_KEY,
            "Counter already exists: " + key);
    }
    
    auto counter = std::make_unique<NamedCounter>(counter_name, initial, opts);
    NamedCounter* ptr = counter.get();
    counters_[key] = std::move(counter);
    
    return make_success(ptr);
}

Result<NamedCounter*> CounterPool::get_counter(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = counters_.find(String(name));
    if (it == counters_.end()) {
        return make_error<NamedCounter*>(ErrorCode::NOTFND,
            "Counter not found: " + String(name));
    }
    
    return make_success(it->second.get());
}

Result<void> CounterPool::delete_counter(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = counters_.find(String(name));
    if (it == counters_.end()) {
        return make_error<void>(ErrorCode::NOTFND,
            "Counter not found: " + String(name));
    }
    
    counters_.erase(it);
    return make_success();
}

bool CounterPool::exists(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return counters_.find(String(name)) != counters_.end();
}

Result<Int64> CounterPool::get(StringView name) {
    auto counter_result = get_counter(name);
    if (counter_result.is_error()) {
        return make_error<Int64>(counter_result.error().code, counter_result.error().message);
    }
    return counter_result.value()->get();
}

Result<Int64> CounterPool::get(StringView name, Int64 increment) {
    auto counter_result = get_counter(name);
    if (counter_result.is_error()) {
        return make_error<Int64>(counter_result.error().code, counter_result.error().message);
    }
    return counter_result.value()->get(increment);
}

Result<void> CounterPool::put(StringView name, Int64 value) {
    auto counter_result = get_counter(name);
    if (counter_result.is_error()) {
        return make_error<void>(counter_result.error().code, counter_result.error().message);
    }
    return counter_result.value()->put(value);
}

std::vector<String> CounterPool::list_counters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<String> names;
    names.reserve(counters_.size());
    for (const auto& [name, counter] : counters_) {
        names.push_back(name);
    }
    return names;
}

std::vector<CounterInfo> CounterPool::list_counter_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CounterInfo> infos;
    infos.reserve(counters_.size());
    for (const auto& [name, counter] : counters_) {
        infos.push_back(counter->get_info());
    }
    return infos;
}

UInt32 CounterPool::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<UInt32>(counters_.size());
}

// =============================================================================
// CounterManager Implementation
// =============================================================================

CounterManager& CounterManager::instance() {
    static CounterManager instance;
    return instance;
}

void CounterManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;
    
    pools_.clear();
    
    // Create default pool
    pools_["DFHCNTR"] = std::make_unique<CounterPool>("DFHCNTR");
    
    reset_stats();
    initialized_ = true;
}

void CounterManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    pools_.clear();
    initialized_ = false;
}

Result<CounterPool*> CounterManager::create_pool(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    String key(name);
    auto it = pools_.find(key);
    if (it != pools_.end()) {
        return make_success(it->second.get());
    }
    
    auto pool = std::make_unique<CounterPool>(name);
    CounterPool* ptr = pool.get();
    pools_[key] = std::move(pool);
    
    return make_success(ptr);
}

Result<CounterPool*> CounterManager::get_pool(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pools_.find(String(name));
    if (it == pools_.end()) {
        return make_error<CounterPool*>(ErrorCode::NOTFND,
            "Pool not found: " + String(name));
    }
    
    return make_success(it->second.get());
}

Result<void> CounterManager::delete_pool(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (name == "DFHCNTR") {
        return make_error<void>(ErrorCode::INVREQ,
            "Cannot delete default pool");
    }
    
    auto it = pools_.find(String(name));
    if (it == pools_.end()) {
        return make_error<void>(ErrorCode::NOTFND, "Pool not found");
    }
    
    pools_.erase(it);
    return make_success();
}

CounterPool* CounterManager::default_pool() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pools_.find("DFHCNTR");
    return (it != pools_.end()) ? it->second.get() : nullptr;
}

Result<Int64> CounterManager::define_counter(StringView name, Int64 initial,
                                              const CounterOptions& opts) {
    CounterPool* pool = default_pool();
    if (!pool) {
        return make_error<Int64>(ErrorCode::NOT_INITIALIZED, "Counter manager not initialized");
    }
    
    auto result = pool->define(name, initial, opts);
    if (result.is_error()) {
        return make_error<Int64>(result.error().code, result.error().message);
    }
    
    ++stats_.counters_defined;
    return make_success(initial);
}

Result<Int64> CounterManager::get(StringView name) {
    CounterPool* pool = default_pool();
    if (!pool) {
        return make_error<Int64>(ErrorCode::NOT_INITIALIZED, "Counter manager not initialized");
    }
    
    auto result = pool->get(name);
    if (result.is_success()) {
        ++stats_.gets_executed;
    }
    return result;
}

Result<Int64> CounterManager::get(StringView name, Int64 increment) {
    CounterPool* pool = default_pool();
    if (!pool) {
        return make_error<Int64>(ErrorCode::NOT_INITIALIZED, "Counter manager not initialized");
    }
    
    auto result = pool->get(name, increment);
    if (result.is_success()) {
        ++stats_.gets_executed;
    }
    return result;
}

Result<void> CounterManager::put(StringView name, Int64 value) {
    CounterPool* pool = default_pool();
    if (!pool) {
        return make_error<void>(ErrorCode::NOT_INITIALIZED, "Counter manager not initialized");
    }
    
    auto result = pool->put(name, value);
    if (result.is_success()) {
        ++stats_.puts_executed;
    }
    return result;
}

Result<Int64> CounterManager::update(StringView name, Int64 expected, Int64 new_value) {
    CounterPool* pool = default_pool();
    if (!pool) {
        return make_error<Int64>(ErrorCode::NOT_INITIALIZED, "Counter manager not initialized");
    }
    
    auto counter_result = pool->get_counter(name);
    if (counter_result.is_error()) {
        return make_error<Int64>(counter_result.error().code, counter_result.error().message);
    }
    
    auto result = counter_result.value()->update(expected, new_value);
    if (result.is_success()) {
        ++stats_.updates_executed;
    }
    return result;
}

Result<void> CounterManager::delete_counter(StringView name) {
    CounterPool* pool = default_pool();
    if (!pool) {
        return make_error<void>(ErrorCode::NOT_INITIALIZED, "Counter manager not initialized");
    }
    
    auto result = pool->delete_counter(name);
    if (result.is_success()) {
        ++stats_.counters_deleted;
    }
    return result;
}

Result<Int64> CounterManager::get(StringView pool_name, StringView name) {
    auto pool_result = get_pool(pool_name);
    if (pool_result.is_error()) {
        return make_error<Int64>(pool_result.error().code, pool_result.error().message);
    }
    
    auto result = pool_result.value()->get(name);
    if (result.is_success()) {
        ++stats_.gets_executed;
    }
    return result;
}

Result<void> CounterManager::put(StringView pool_name, StringView name, Int64 value) {
    auto pool_result = get_pool(pool_name);
    if (pool_result.is_error()) {
        return make_error<void>(pool_result.error().code, pool_result.error().message);
    }
    
    auto result = pool_result.value()->put(name, value);
    if (result.is_success()) {
        ++stats_.puts_executed;
    }
    return result;
}

Result<CounterInfo> CounterManager::query(StringView name) {
    CounterPool* pool = default_pool();
    if (!pool) {
        return make_error<CounterInfo>(ErrorCode::NOT_INITIALIZED, "Counter manager not initialized");
    }
    
    auto counter_result = pool->get_counter(name);
    if (counter_result.is_error()) {
        return make_error<CounterInfo>(counter_result.error().code, counter_result.error().message);
    }
    
    return make_success(counter_result.value()->get_info());
}

std::vector<String> CounterManager::list_pools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<String> names;
    names.reserve(pools_.size());
    for (const auto& [name, pool] : pools_) {
        names.push_back(name);
    }
    return names;
}

CounterStats CounterManager::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void CounterManager::reset_stats() {
    stats_ = CounterStats{};
}

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

Result<Int64> exec_cics_define_counter(StringView name, Int64 initial) {
    return CounterManager::instance().define_counter(name, initial);
}

Result<Int64> exec_cics_define_counter(StringView name, Int64 initial,
                                        const CounterOptions& opts) {
    return CounterManager::instance().define_counter(name, initial, opts);
}

Result<Int64> exec_cics_get_counter(StringView name) {
    return CounterManager::instance().get(name);
}

Result<Int64> exec_cics_get_counter(StringView name, Int64 increment) {
    return CounterManager::instance().get(name, increment);
}

Result<Int64> exec_cics_get_counter(StringView pool, StringView name) {
    return CounterManager::instance().get(pool, name);
}

Result<void> exec_cics_put_counter(StringView name, Int64 value) {
    return CounterManager::instance().put(name, value);
}

Result<void> exec_cics_put_counter(StringView pool, StringView name, Int64 value) {
    return CounterManager::instance().put(pool, name, value);
}

Result<Int64> exec_cics_update_counter(StringView name, Int64 expected, Int64 new_value) {
    return CounterManager::instance().update(name, expected, new_value);
}

Result<void> exec_cics_delete_counter(StringView name) {
    return CounterManager::instance().delete_counter(name);
}

Result<CounterInfo> exec_cics_query_counter(StringView name) {
    return CounterManager::instance().query(name);
}

} // namespace counter
} // namespace cics
