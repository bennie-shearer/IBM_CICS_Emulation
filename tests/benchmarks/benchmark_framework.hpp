#pragma once
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <algorithm>

namespace cics::benchmark {

struct BenchmarkResult {
    std::string name;
    double min_ns = 0;
    double max_ns = 0;
    double avg_ns = 0;
    double median_ns = 0;
    size_t iterations = 0;
    double ops_per_sec = 0;
};

class Benchmark {
public:
    using BenchFunc = std::function<void()>;
    
    Benchmark(std::string name, size_t iterations = 10000)
        : name_(std::move(name)), iterations_(iterations) {}
    
    template<typename Func>
    BenchmarkResult run(Func&& func) {
        std::vector<double> times;
        times.reserve(iterations_);
        
        // Warmup
        for (size_t i = 0; i < std::min(iterations_ / 10, size_t(100)); ++i) {
            func();
        }
        
        // Actual benchmark
        for (size_t i = 0; i < iterations_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            func();
            auto end = std::chrono::high_resolution_clock::now();
            times.push_back(std::chrono::duration<double, std::nano>(end - start).count());
        }
        
        std::sort(times.begin(), times.end());
        
        BenchmarkResult result;
        result.name = name_;
        result.iterations = iterations_;
        result.min_ns = times.front();
        result.max_ns = times.back();
        result.avg_ns = std::accumulate(times.begin(), times.end(), 0.0) / static_cast<double>(times.size());
        result.median_ns = times[times.size() / 2];
        result.ops_per_sec = 1e9 / result.avg_ns;
        
        return result;
    }
    
    static void print_result(const BenchmarkResult& r) {
        std::cout << std::left << std::setw(40) << r.name
                  << " | " << std::right << std::setw(12) << std::fixed << std::setprecision(2)
                  << r.avg_ns << " ns"
                  << " | " << std::setw(12) << r.median_ns << " ns"
                  << " | " << std::setw(12) << std::setprecision(0) << r.ops_per_sec << " ops/s"
                  << "\n";
    }
    
    static void print_header() {
        std::cout << std::left << std::setw(40) << "Benchmark"
                  << " | " << std::right << std::setw(15) << "Avg"
                  << " | " << std::setw(15) << "Median"
                  << " | " << std::setw(15) << "Throughput"
                  << "\n";
        std::cout << std::string(90, '-') << "\n";
    }
    
private:
    std::string name_;
    size_t iterations_;
};

} // namespace cics::benchmark
