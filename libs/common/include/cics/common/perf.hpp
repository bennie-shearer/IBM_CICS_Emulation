// =============================================================================
// CICS Emulation - Performance Monitoring
// Version: 3.4.6
// =============================================================================

#pragma once

#include <chrono>
#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace cics::perf {

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::nanoseconds;
using TimePoint = Clock::time_point;

/**
 * @brief RAII-based scope timer for measuring code block execution
 */
class ScopeTimer {
public:
    explicit ScopeTimer(const std::string& name);
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

    /**
     * @brief Get elapsed time so far
     */
    Duration elapsed() const {
        return Clock::now() - start_;
    }

    /**
     * @brief Get elapsed time in milliseconds
     */
    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(elapsed()).count();
    }

private:
    std::string name_;
    TimePoint start_;
};

/**
 * @brief Statistics for a metric
 */
struct MetricStats {
    int64_t count = 0;
    double total = 0.0;
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::lowest();
    double mean = 0.0;
    double variance = 0.0;
    double std_dev = 0.0;
    double p50 = 0.0;  // median
    double p90 = 0.0;
    double p99 = 0.0;

    std::string to_string() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "count=" << count
            << " total=" << total << "ms"
            << " min=" << min << "ms"
            << " max=" << max << "ms"
            << " mean=" << mean << "ms"
            << " stddev=" << std_dev << "ms"
            << " p50=" << p50 << "ms"
            << " p90=" << p90 << "ms"
            << " p99=" << p99 << "ms";
        return oss.str();
    }
};

/**
 * @brief Performance metrics collector
 */
class MetricsCollector {
public:
    static MetricsCollector& instance() {
        static MetricsCollector inst;
        return inst;
    }

    /**
     * @brief Record a timing measurement (in nanoseconds)
     */
    void record(const std::string& name, Duration duration) {
        std::lock_guard<std::mutex> lock(mutex_);
        double ms = std::chrono::duration<double, std::milli>(duration).count();
        samples_[name].push_back(ms);
        
        // Keep only last N samples to prevent unbounded growth
        const size_t MAX_SAMPLES = 10000;
        if (samples_[name].size() > MAX_SAMPLES) {
            samples_[name].erase(samples_[name].begin(), 
                                  samples_[name].begin() + (samples_[name].size() - MAX_SAMPLES));
        }
    }

    /**
     * @brief Record a timing measurement (in milliseconds)
     */
    void record_ms(const std::string& name, double ms) {
        record(name, std::chrono::duration_cast<Duration>(
            std::chrono::duration<double, std::milli>(ms)));
    }

    /**
     * @brief Increment a counter
     */
    void increment(const std::string& name, int64_t delta = 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        counters_[name] += delta;
    }

    /**
     * @brief Set a gauge value
     */
    void gauge(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        gauges_[name] = value;
    }

    /**
     * @brief Get statistics for a metric
     */
    MetricStats get_stats(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        MetricStats stats;

        auto it = samples_.find(name);
        if (it == samples_.end() || it->second.empty()) {
            return stats;
        }

        std::vector<double> sorted = it->second;
        std::sort(sorted.begin(), sorted.end());

        stats.count = static_cast<int64_t>(sorted.size());
        stats.total = std::accumulate(sorted.begin(), sorted.end(), 0.0);
        stats.min = sorted.front();
        stats.max = sorted.back();
        stats.mean = stats.total / static_cast<double>(stats.count);

        // Variance and std dev
        double sum_sq = 0.0;
        for (double v : sorted) {
            sum_sq += (v - stats.mean) * (v - stats.mean);
        }
        stats.variance = sum_sq / static_cast<double>(stats.count);
        stats.std_dev = std::sqrt(stats.variance);

        // Percentiles
        stats.p50 = percentile(sorted, 50);
        stats.p90 = percentile(sorted, 90);
        stats.p99 = percentile(sorted, 99);

        return stats;
    }

    /**
     * @brief Get counter value
     */
    int64_t get_counter(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = counters_.find(name);
        return it != counters_.end() ? it->second : 0;
    }

    /**
     * @brief Get gauge value
     */
    double get_gauge(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = gauges_.find(name);
        return it != gauges_.end() ? it->second : 0.0;
    }

    /**
     * @brief Get all metric names
     */
    std::vector<std::string> get_metric_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [name, _] : samples_) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * @brief Get all counter names
     */
    std::vector<std::string> get_counter_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [name, _] : counters_) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * @brief Generate a summary report
     */
    std::string report() const {
        std::ostringstream oss;
        oss << "=== Performance Metrics Report ===\n\n";

        oss << "-- Timings --\n";
        for (const auto& name : get_metric_names()) {
            oss << name << ": " << get_stats(name).to_string() << "\n";
        }

        oss << "\n-- Counters --\n";
        for (const auto& name : get_counter_names()) {
            oss << name << ": " << get_counter(name) << "\n";
        }

        oss << "\n-- Gauges --\n";
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, value] : gauges_) {
            oss << name << ": " << value << "\n";
        }

        return oss.str();
    }

    /**
     * @brief Clear all metrics
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.clear();
        counters_.clear();
        gauges_.clear();
    }

private:
    MetricsCollector() = default;
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    static double percentile(const std::vector<double>& sorted, int p) {
        if (sorted.empty()) return 0.0;
        double idx = (static_cast<double>(p) / 100.0) * static_cast<double>(sorted.size() - 1);
        size_t lower = static_cast<size_t>(std::floor(idx));
        size_t upper = static_cast<size_t>(std::ceil(idx));
        if (lower == upper) return sorted[lower];
        double frac = idx - static_cast<double>(lower);
        return sorted[lower] * (1 - frac) + sorted[upper] * frac;
    }

    mutable std::mutex mutex_;
    std::map<std::string, std::vector<double>> samples_;
    std::map<std::string, int64_t> counters_;
    std::map<std::string, double> gauges_;
};

// Implementation of ScopeTimer
inline ScopeTimer::ScopeTimer(const std::string& name)
    : name_(name), start_(Clock::now()) {}

inline ScopeTimer::~ScopeTimer() {
    MetricsCollector::instance().record(name_, elapsed());
}

/**
 * @brief Convenience macro for timing a scope
 */
#define CICS_TIMED_SCOPE(name) \
    cics::perf::ScopeTimer _timer_##__LINE__(name)

/**
 * @brief Convenience macro for timing a function
 */
#define CICS_TIMED_FUNCTION() \
    CICS_TIMED_SCOPE(__FUNCTION__)

} // namespace cics::perf
