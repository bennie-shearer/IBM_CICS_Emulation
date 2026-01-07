// =============================================================================
// CICS Emulation - Interval Control Example
// Version: 3.4.6
// =============================================================================
// Demonstrates ASKTIME, DELAY, POST, WAIT, START, and CANCEL operations
// =============================================================================

#include <cics/interval/interval_control.hpp>
#include <iostream>
#include <iomanip>
#include <thread>

using namespace cics;
using namespace cics::interval;

void print_header(const char* title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void print_time_info(const TimeInfo& info) {
    std::cout << "  Date:         " << info.date << " (YYYYMMDD)\n";
    std::cout << "  Time:         " << std::setfill('0') << std::setw(6) << info.time << " (HHMMSS)\n";
    std::cout << "  Year:         " << info.year << "\n";
    std::cout << "  Month:        " << info.month << "\n";
    std::cout << "  Day of Month: " << info.dayofmonth << "\n";
    std::cout << "  Day of Week:  " << info.dayofweek << " (0=Sunday)\n";
    std::cout << "  Milliseconds: " << info.milliseconds << "\n";
    std::cout << "  Abstime:      " << info.abstime.to_string() << "\n";
}

int main() {
    std::cout << R"(
+==============================================================+
|     CICS Emulation - Interval Control Example     |
|                        Version 3.4.6                         |
+==============================================================+
)";

    // Initialize the interval control manager
    auto& icm = IntervalControlManager::instance();
    icm.initialize();

    // =========================================================================
    // 1. ASKTIME - Get Current Time
    // =========================================================================
    print_header("1. ASKTIME - Get Current Time");
    
    auto time_result = exec_cics_asktime();
    if (time_result.is_success()) {
        print_time_info(time_result.value());
    }

    // Using direct manager call
    auto abstime_result = icm.asktime_abstime();
    if (abstime_result.is_success()) {
        std::cout << "\n  ABSTIME value: " << abstime_result.value().value << "\n";
        std::cout << "  HHMMSS:        " << abstime_result.value().to_hhmmss() << "\n";
        std::cout << "  YYYYMMDD:      " << abstime_result.value().to_yyyymmdd() << "\n";
    }

    // =========================================================================
    // 2. DELAY - Suspend Task
    // =========================================================================
    print_header("2. DELAY - Suspend Task");
    
    std::cout << "  Delaying for 1 second...\n";
    auto start = std::chrono::steady_clock::now();
    
    icm.delay_for(std::chrono::milliseconds(1000));
    
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Actual delay: " << elapsed.count() << " ms\n";

    // Delay using HHMMSS format
    std::cout << "\n  Delaying using INTERVAL(000001) - 1 second...\n";
    start = std::chrono::steady_clock::now();
    
    exec_cics_delay_interval(1);  // 000001 = 1 second
    
    end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Actual delay: " << elapsed.count() << " ms\n";

    // =========================================================================
    // 3. POST and WAIT - Event Synchronization
    // =========================================================================
    print_header("3. POST and WAIT - Event Synchronization");
    
    // Create an event
    auto event_result = icm.create_event();
    if (!event_result.is_success()) {
        std::cerr << "  Failed to create event\n";
        return 1;
    }
    UInt32 event_id = event_result.value();
    std::cout << "  Created event ID: " << event_id << "\n";

    // Start a thread to post the event after a delay
    std::thread poster([&icm, event_id]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "  [Thread] Posting event " << event_id << "...\n";
        icm.post(event_id);
    });

    // Wait for the event
    std::cout << "  Waiting for event (timeout 5 seconds)...\n";
    auto wait_result = icm.wait_event(event_id, IntervalSpec::interval(0, 0, 5));
    
    if (wait_result.is_success()) {
        std::cout << "  Event " << wait_result.value() << " was posted!\n";
    } else {
        std::cout << "  Wait timed out or failed: " << wait_result.error().message << "\n";
    }
    
    poster.join();
    icm.delete_event(event_id);

    // =========================================================================
    // 4. START - Schedule Transaction
    // =========================================================================
    print_header("4. START - Schedule Transaction");
    
    // Set a callback for when transactions are started
    icm.set_transaction_callback([](const StartRequest& req) {
        std::cout << "  [Callback] Transaction started: " << req.to_string() << "\n";
        if (!req.data.empty()) {
            std::cout << "    Data length: " << req.data.size() << " bytes\n";
        }
    });

    // Schedule a transaction
    std::cout << "  Scheduling transaction 'TRNA' to start in 1 second...\n";
    ByteBuffer data = {'H', 'E', 'L', 'L', 'O'};
    auto start_result = exec_cics_start("TRNA", 1, ConstByteSpan(data.data(), data.size()));
    
    if (start_result.is_success()) {
        std::cout << "  Request ID: " << start_result.value() << "\n";
    }

    // Wait for the scheduled transaction
    std::cout << "  Waiting for scheduled transaction...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // =========================================================================
    // 5. CANCEL - Cancel Scheduled Transaction
    // =========================================================================
    print_header("5. CANCEL - Cancel Scheduled Transaction");
    
    // Schedule another transaction
    std::cout << "  Scheduling transaction 'TRNB' to start in 5 seconds...\n";
    auto start2_result = exec_cics_start("TRNB", 5);
    
    if (start2_result.is_success()) {
        UInt32 req_id = start2_result.value();
        std::cout << "  Request ID: " << req_id << "\n";
        
        // Cancel it
        std::cout << "  Cancelling request " << req_id << "...\n";
        auto cancel_result = exec_cics_cancel(req_id);
        
        if (cancel_result.is_success()) {
            std::cout << "  Successfully cancelled!\n";
        }
    }

    // =========================================================================
    // 6. Statistics
    // =========================================================================
    print_header("6. Statistics");
    std::cout << icm.get_statistics();

    // Shutdown
    icm.shutdown();

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " Interval Control Example Complete\n";
    std::cout << std::string(60, '=') << "\n";

    return 0;
}
