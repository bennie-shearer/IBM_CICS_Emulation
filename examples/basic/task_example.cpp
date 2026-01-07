// =============================================================================
// CICS Emulation - Task Control Example
// Version: 3.4.6
// =============================================================================
// Demonstrates ENQ, DEQ, SUSPEND, and resource serialization
// =============================================================================

#include <cics/task/task_control.hpp>
#include <iostream>
#include <thread>
#include <vector>

using namespace cics;
using namespace cics::task;

void print_header(const char* title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

int main() {
    std::cout << R"(
+==============================================================+
|       CICS Emulation - Task Control Example       |
|                        Version 3.4.6                         |
+==============================================================+
)";

    auto& tcm = TaskControlManager::instance();

    // =========================================================================
    // 1. Create Tasks
    // =========================================================================
    print_header("1. Create Tasks");
    
    FixedString<4> trans1, trans2;
    trans1 = StringView("TRN1");
    trans2 = StringView("TRN2");
    
    auto task1_result = tcm.create_task(trans1);
    if (task1_result.is_success()) {
        std::cout << "  Created Task 1 with ID: " << task1_result.value() << "\n";
    }

    // =========================================================================
    // 2. ENQ - Enqueue Resources
    // =========================================================================
    print_header("2. ENQ - Enqueue Resources");
    
    // Enqueue an exclusive lock
    std::cout << "  Enqueueing exclusive lock on 'CUSTOMER-FILE'...\n";
    auto enq_result = exec_cics_enq("CUSTOMER-FILE");
    if (enq_result.is_success()) {
        std::cout << "  Lock acquired successfully!\n";
    }

    // Enqueue with length
    std::cout << "\n  Enqueueing lock on 'ACCOUNT' with length 8...\n";
    auto enq2_result = tcm.enq("ACCOUNT", 8);
    if (enq2_result.is_success()) {
        std::cout << "  Lock acquired successfully!\n";
    }

    // Check lock status
    std::cout << "\n  Checking lock status:\n";
    std::cout << "    'CUSTOMER-FILE' locked: " 
              << (tcm.is_locked("CUSTOMER-FILE") ? "YES" : "NO") << "\n";
    std::cout << "    'ACCOUNT' locked: " 
              << (tcm.is_locked("ACCOUNT") ? "YES" : "NO") << "\n";
    std::cout << "    'OTHER-FILE' locked: " 
              << (tcm.is_locked("OTHER-FILE") ? "YES" : "NO") << "\n";

    // =========================================================================
    // 3. ResourceLock RAII Guard
    // =========================================================================
    print_header("3. ResourceLock RAII Guard");
    
    {
        std::cout << "  Creating scoped lock on 'TEMP-RESOURCE'...\n";
        ResourceLock lock("TEMP-RESOURCE");
        
        if (lock.is_locked()) {
            std::cout << "  Lock acquired in scope\n";
            std::cout << "    'TEMP-RESOURCE' locked: " 
                      << (tcm.is_locked("TEMP-RESOURCE") ? "YES" : "NO") << "\n";
        }
        
        std::cout << "  Leaving scope...\n";
    }
    
    std::cout << "  After scope:\n";
    std::cout << "    'TEMP-RESOURCE' locked: " 
              << (tcm.is_locked("TEMP-RESOURCE") ? "YES" : "NO") << "\n";

    // =========================================================================
    // 4. NOSUSPEND Option
    // =========================================================================
    print_header("4. NOSUSPEND Option");
    
    std::cout << "  Trying to enqueue 'CUSTOMER-FILE' with NOSUSPEND...\n";
    auto nosuspend_result = exec_cics_enq_nosuspend("CUSTOMER-FILE");
    
    if (nosuspend_result.is_success()) {
        std::cout << "  Lock acquired!\n";
    } else {
        std::cout << "  Lock failed (expected - already held): " 
                  << nosuspend_result.error().message << "\n";
    }

    // =========================================================================
    // 5. DEQ - Dequeue Resources
    // =========================================================================
    print_header("5. DEQ - Dequeue Resources");
    
    std::cout << "  Releasing 'CUSTOMER-FILE' lock...\n";
    auto deq_result = exec_cics_deq("CUSTOMER-FILE");
    if (deq_result.is_success()) {
        std::cout << "  Lock released!\n";
    }

    std::cout << "\n  After DEQ:\n";
    std::cout << "    'CUSTOMER-FILE' locked: " 
              << (tcm.is_locked("CUSTOMER-FILE") ? "YES" : "NO") << "\n";

    // =========================================================================
    // 6. SUSPEND - Suspend Task
    // =========================================================================
    print_header("6. SUSPEND - Suspend Task");
    
    std::cout << "  Suspending task briefly...\n";
    auto start = std::chrono::steady_clock::now();
    
    tcm.suspend(std::chrono::milliseconds(100));
    
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Resumed after " << elapsed.count() << " ms\n";

    // =========================================================================
    // 7. Multi-threaded Resource Contention
    // =========================================================================
    print_header("7. Multi-threaded Resource Contention");
    
    std::atomic<int> counter{0};
    std::vector<std::thread> threads;
    
    std::cout << "  Starting 5 threads competing for 'SHARED-COUNTER'...\n\n";
    
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&tcm, &counter, i]() {
            // Create a task for this thread
            FixedString<4> trans;
            trans = StringView("THR" + std::to_string(i));
            tcm.create_task(trans);
            
            for (int j = 0; j < 3; ++j) {
                // Enqueue the resource
                auto result = tcm.enq("SHARED-COUNTER");
                if (result.is_success()) {
                    int old_value = counter.load();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    counter.store(old_value + 1);
                    
                    std::cout << "    Thread " << i << " incremented counter to " 
                              << counter.load() << "\n";
                    
                    tcm.deq("SHARED-COUNTER");
                }
            }
            
            tcm.end_current_task();
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "\n  Final counter value: " << counter.load() << " (expected: 15)\n";

    // =========================================================================
    // 8. Task and Lock Listing
    // =========================================================================
    print_header("8. Task and Lock Listing");
    
    // Create some tasks for listing
    FixedString<4> trans3;
    trans3 = StringView("LST1");
    tcm.create_task(trans3);
    tcm.enq("RESOURCE-A");
    tcm.enq("RESOURCE-B");
    
    std::cout << "  Active tasks: " << tcm.task_count() << "\n";
    std::cout << "  Active locks: " << tcm.lock_count() << "\n";
    
    auto tasks = tcm.list_tasks();
    std::cout << "\n  Task List:\n";
    for (const auto& task : tasks) {
        std::cout << "    " << task.to_string() << "\n";
    }
    
    auto locks = tcm.list_locks();
    std::cout << "\n  Lock List:\n";
    for (const auto& lock : locks) {
        std::cout << "    " << lock.to_string() << "\n";
    }

    // =========================================================================
    // 9. Statistics
    // =========================================================================
    print_header("9. Statistics");
    std::cout << tcm.get_statistics();

    // Cleanup
    tcm.deq_all();
    tcm.end_current_task();

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " Task Control Example Complete\n";
    std::cout << std::string(60, '=') << "\n";

    return 0;
}
