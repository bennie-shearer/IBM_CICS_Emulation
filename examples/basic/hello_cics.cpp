// =============================================================================
// CICS Emulation - Hello CICS Example
// A simple introduction to the CICS emulation framework
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/cics/cics_types.hpp"
#include <iostream>

int main() {
    std::cout << "Hello from CICS Emulation!\n\n";
    
    // Create an Execute Interface Block (EIB)
    cics::cics::EIB eib;
    eib.set_time_date();
    eib.eibtrnid = cics::FixedString<4>("HELO");
    
    std::cout << "Transaction: " << eib.eibtrnid.trimmed() << "\n";
    std::cout << "Time: " << eib.eibtime << "\n";
    std::cout << "Date: " << eib.eibdate << "\n";
    
    // Create a COMMAREA
    cics::cics::Commarea comm(80);
    comm.set_string(0, "Hello from CICS Emulation!", 40);
    
    std::cout << "\nCOMMAREA: " << comm.get_string(0, 40) << "\n";
    
    return 0;
}
