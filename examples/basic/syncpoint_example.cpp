// =============================================================================
// CICS Emulation - Syncpoint Control Example
// =============================================================================
// Demonstrates SYNCPOINT and ROLLBACK functionality for transaction management.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#include <cics/syncpoint/syncpoint.hpp>
#include <iostream>
#include <iomanip>

using namespace cics;
using namespace cics::syncpoint;

void print_header(const char* title) {
    std::cout << "\n============================================================\n";
    std::cout << " " << title << "\n";
    std::cout << "============================================================\n";
}

void print_uow_info(const UOWInfo& info) {
    std::cout << "  UOW ID: " << info.uow_id << "\n";
    std::cout << "  State: " << uow_state_to_string(info.state) << "\n";
    std::cout << "  Resources: " << info.resource_count << "\n";
    std::cout << "  Syncpoints: " << info.syncpoint_count << "\n";
    std::cout << "  Rollbacks: " << info.rollback_count << "\n";
}

int main() {
    std::cout << "\n";
    std::cout << "+==============================================================+\n";
    std::cout << "|    CICS Emulation - Syncpoint Control Example     |\n";
    std::cout << "|                        Version 3.4.6                         |\n";
    std::cout << "+==============================================================+\n";

    // Initialize
    SyncpointManager::instance().initialize();

    // =========================================================================
    // 1. Basic Syncpoint
    // =========================================================================
    print_header("1. Basic Syncpoint");
    
    {
        // Begin a Unit of Work
        auto uow_result = SyncpointManager::instance().begin_uow();
        if (uow_result.is_success()) {
            std::cout << "  Started UOW: " << uow_result.value() << "\n";
            
            // Register some resources
            SyncpointManager::instance().register_resource(
                "RESOURCE1", ResourceType::VSAM_FILE,
                []() -> Result<void> { 
                    std::cout << "  [RESOURCE1] Preparing...\n";
                    return make_success(); 
                },
                []() -> Result<void> { 
                    std::cout << "  [RESOURCE1] Committing...\n";
                    return make_success(); 
                },
                []() -> Result<void> { 
                    std::cout << "  [RESOURCE1] Rolling back...\n";
                    return make_success(); 
                }
            );
            
            SyncpointManager::instance().register_resource(
                "RESOURCE2", ResourceType::TSQ,
                []() -> Result<void> { 
                    std::cout << "  [RESOURCE2] Preparing...\n";
                    return make_success(); 
                },
                []() -> Result<void> { 
                    std::cout << "  [RESOURCE2] Committing...\n";
                    return make_success(); 
                },
                []() -> Result<void> { 
                    std::cout << "  [RESOURCE2] Rolling back...\n";
                    return make_success(); 
                }
            );
            
            // Issue syncpoint
            std::cout << "\n  Issuing SYNCPOINT...\n";
            auto sync_result = exec_cics_syncpoint();
            if (sync_result.is_success()) {
                std::cout << "  Syncpoint successful!\n";
            }
        }
    }

    // =========================================================================
    // 2. Syncpoint with Rollback
    // =========================================================================
    print_header("2. Syncpoint with Rollback");
    
    {
        auto uow_result = SyncpointManager::instance().begin_uow();
        if (uow_result.is_success()) {
            std::cout << "  Started UOW: " << uow_result.value() << "\n";
            
            // Register resource
            SyncpointManager::instance().register_resource(
                "ACCOUNT", ResourceType::VSAM_FILE,
                []() -> Result<void> { 
                    std::cout << "  [ACCOUNT] Preparing account update...\n";
                    return make_success(); 
                },
                []() -> Result<void> { 
                    std::cout << "  [ACCOUNT] Committing account update...\n";
                    return make_success(); 
                },
                []() -> Result<void> { 
                    std::cout << "  [ACCOUNT] Rolling back account update...\n";
                    return make_success(); 
                }
            );
            
            // Simulate error condition
            std::cout << "\n  Simulating error condition...\n";
            std::cout << "  Issuing ROLLBACK...\n";
            
            auto rollback_result = exec_cics_syncpoint_rollback();
            if (rollback_result.is_success()) {
                std::cout << "  Rollback successful!\n";
            }
        }
    }

    // =========================================================================
    // 3. SyncpointGuard RAII
    // =========================================================================
    print_header("3. SyncpointGuard RAII Pattern");
    
    {
        std::cout << "  Creating SyncpointGuard (auto-commit=true)...\n";
        SyncpointGuard guard(true);  // Auto-commit on success
        
        if (guard.is_active()) {
            std::cout << "  Guard active with UOW: " << guard.uow_id() << "\n";
            
            // Do some work...
            std::cout << "  Performing transactional work...\n";
            
            // Explicitly commit
            auto result = guard.commit();
            if (result.is_success()) {
                std::cout << "  Committed successfully!\n";
            }
        }
    }
    
    {
        std::cout << "\n  Creating SyncpointGuard (auto-commit=false)...\n";
        SyncpointGuard guard(false);  // Auto-rollback on destruction
        
        if (guard.is_active()) {
            std::cout << "  Guard active with UOW: " << guard.uow_id() << "\n";
            std::cout << "  Simulating error - guard will auto-rollback...\n";
            // Guard goes out of scope, auto-rollback
        }
    }

    // =========================================================================
    // 4. Statistics
    // =========================================================================
    print_header("4. Syncpoint Statistics");
    
    auto stats = SyncpointManager::instance().get_stats();
    std::cout << "  UOWs Created:        " << stats.uows_created << "\n";
    std::cout << "  UOWs Committed:      " << stats.uows_committed << "\n";
    std::cout << "  UOWs Rolled Back:    " << stats.uows_rolled_back << "\n";
    std::cout << "  Syncpoints Issued:   " << stats.syncpoints_issued << "\n";
    std::cout << "  Rollbacks Issued:    " << stats.rollbacks_issued << "\n";
    std::cout << "  Resources Registered:" << stats.resources_registered << "\n";

    // Cleanup
    SyncpointManager::instance().shutdown();

    std::cout << "\n================================================================\n";
    std::cout << "Syncpoint example completed successfully!\n";
    std::cout << "================================================================\n\n";

    return 0;
}
