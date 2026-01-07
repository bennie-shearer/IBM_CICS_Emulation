#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace cics::test {

struct TestResult {
    std::string name;
    bool passed = false;
    std::string message;
    double duration_ms = 0.0;
};

class TestSuite {
private:
    std::string name_;
    std::vector<std::pair<std::string, std::function<void()>>> tests_;
    std::vector<TestResult> results_;
    int passed_ = 0, failed_ = 0;
    
public:
    explicit TestSuite(std::string name) : name_(std::move(name)) {}
    
    void add_test(std::string name, std::function<void()> test) {
        tests_.emplace_back(std::move(name), std::move(test));
    }
    
    void run() {
        std::cout << "\n=== " << name_ << " ===\n";
        
        for (const auto& [name, test] : tests_) {
            TestResult result;
            result.name = name;
            
            auto start = std::chrono::high_resolution_clock::now();
            try {
                test();
                result.passed = true;
                passed_++;
            } catch (const std::exception& e) {
                result.passed = false;
                result.message = e.what();
                failed_++;
            }
            auto end = std::chrono::high_resolution_clock::now();
            result.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
            
            std::cout << (result.passed ? "[PASS]" : "[FAIL]") << " " << name;
            std::cout << " (" << std::fixed << std::setprecision(2) << result.duration_ms << "ms)";
            if (!result.passed) std::cout << "\n       " << result.message;
            std::cout << "\n";
            
            results_.push_back(std::move(result));
        }
        
        std::cout << "\nResults: " << passed_ << " passed, " << failed_ << " failed\n";
    }
    
    [[nodiscard]] int passed() const { return passed_; }
    [[nodiscard]] int failed() const { return failed_; }
    [[nodiscard]] const std::vector<TestResult>& results() const { return results_; }
};

class TestRunner {
private:
    std::vector<TestSuite*> suites_;
    
public:
    void add_suite(TestSuite* suite) { suites_.push_back(suite); }
    
    int run_all() {
        int total_passed = 0, total_failed = 0;
        
        std::cout << "\n+==============================================================+\n";
        std::cout << "|           CICS Emulation Test Runner              |\n";
        std::cout << "+==============================================================+\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (auto* suite : suites_) {
            suite->run();
            total_passed += suite->passed();
            total_failed += suite->failed();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        
        std::cout << "\n==============================================================\n";
        std::cout << "Total: " << total_passed << " passed, " << total_failed << " failed";
        std::cout << " (" << std::fixed << std::setprecision(2) << total_ms << "ms)\n";
        std::cout << "==============================================================\n";
        
        return total_failed;
    }
};

// Assertion macros
#define ASSERT_TRUE(cond) do { if (!(cond)) throw std::runtime_error("Assertion failed: " #cond); } while(0)
#define ASSERT_FALSE(cond) do { if (cond) throw std::runtime_error("Assertion failed: NOT " #cond); } while(0)
#define ASSERT_EQ(a, b) do { if (!((a) == (b))) throw std::runtime_error("Assertion failed: " #a " == " #b); } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) throw std::runtime_error("Assertion failed: " #a " != " #b); } while(0)
#define ASSERT_LT(a, b) do { if (!((a) < (b))) throw std::runtime_error("Assertion failed: " #a " < " #b); } while(0)
#define ASSERT_LE(a, b) do { if (!((a) <= (b))) throw std::runtime_error("Assertion failed: " #a " <= " #b); } while(0)
#define ASSERT_GT(a, b) do { if (!((a) > (b))) throw std::runtime_error("Assertion failed: " #a " > " #b); } while(0)
#define ASSERT_GE(a, b) do { if (!((a) >= (b))) throw std::runtime_error("Assertion failed: " #a " >= " #b); } while(0)
#define ASSERT_THROW(expr, exc) do { bool caught = false; try { expr; } catch (const exc&) { caught = true; } if (!caught) throw std::runtime_error("Expected exception: " #exc); } while(0)
#define ASSERT_NO_THROW(expr) do { try { expr; } catch (...) { throw std::runtime_error("Unexpected exception in: " #expr); } } while(0)

} // namespace cics::test
