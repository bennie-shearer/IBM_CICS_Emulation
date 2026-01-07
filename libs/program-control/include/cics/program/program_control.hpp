// =============================================================================
// CICS Emulation - Program Control Module
// Version: 3.4.6
// =============================================================================
// Implements CICS Program Control services:
// - LINK: Link to another program
// - XCTL: Transfer control to another program
// - RETURN: Return to calling program
// - LOAD: Load a program into storage
// - RELEASE: Release a loaded program
// =============================================================================

#ifndef CICS_PROGRAM_CONTROL_HPP
#define CICS_PROGRAM_CONTROL_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <stack>
#include <memory>

namespace cics {
namespace program {

// =============================================================================
// Program Types
// =============================================================================

enum class ProgramType : UInt8 {
    ASSEMBLER,
    COBOL,
    PLI,
    C,
    CPP,
    JAVA,
    NATIVE,      // Native function pointer
    UNKNOWN
};

enum class ProgramStatus : UInt8 {
    NOT_LOADED,
    LOADED,
    ENABLED,
    DISABLED,
    NEW_COPY
};

// =============================================================================
// Program Definition
// =============================================================================

// Function signature for program entry point
using ProgramFunction = std::function<Int32(void* commarea, UInt32 commarea_length)>;

struct ProgramDefinition {
    FixedString<8> program_name;
    ProgramType type = ProgramType::NATIVE;
    ProgramStatus status = ProgramStatus::NOT_LOADED;
    ProgramFunction entry_point;
    void* load_address = nullptr;
    UInt32 program_size = 0;
    UInt32 use_count = 0;
    UInt32 load_count = 0;
    std::chrono::steady_clock::time_point load_time;
    String description;
    String language;
    bool resident = false;  // Keep in storage
    
    [[nodiscard]] bool is_loaded() const { 
        return status == ProgramStatus::LOADED || status == ProgramStatus::ENABLED; 
    }
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// Program Link Level
// =============================================================================

struct LinkLevel {
    FixedString<8> program_name;
    void* commarea = nullptr;
    UInt32 commarea_length = 0;
    void* return_address = nullptr;
    Int32 response_code = 0;
    std::chrono::steady_clock::time_point entry_time;
};

// =============================================================================
// Program Control Manager
// =============================================================================

class ProgramControlManager {
private:
    mutable std::mutex mutex_;
    
    // Program definitions (PPT - Processing Program Table)
    std::unordered_map<String, ProgramDefinition> programs_;
    
    // Current program stack per thread
    thread_local static std::stack<LinkLevel> link_stack_;
    thread_local static FixedString<8> current_program_;
    
    // Statistics
    struct Statistics {
        UInt64 link_count = 0;
        UInt64 xctl_count = 0;
        UInt64 return_count = 0;
        UInt64 load_count = 0;
        UInt64 release_count = 0;
        UInt64 program_not_found = 0;
        UInt32 max_link_depth = 0;
    } stats_;
    
public:
    ProgramControlManager() = default;
    ~ProgramControlManager() = default;
    
    // Singleton access
    static ProgramControlManager& instance();
    
    // Program definition management
    Result<void> define_program(const ProgramDefinition& def);
    Result<void> define_program(StringView name, ProgramFunction entry_point);
    Result<void> undefine_program(StringView name);
    Result<ProgramDefinition*> get_program(StringView name);
    [[nodiscard]] bool program_exists(StringView name) const;
    [[nodiscard]] std::vector<String> list_programs() const;
    
    // LINK - Link to program (returns control)
    Result<Int32> link(StringView program_name);
    Result<Int32> link(StringView program_name, void* commarea, UInt32 commarea_length);
    Result<Int32> link(StringView program_name, ByteBuffer& commarea);
    
    // XCTL - Transfer control (does not return)
    Result<void> xctl(StringView program_name);
    Result<void> xctl(StringView program_name, void* commarea, UInt32 commarea_length);
    Result<void> xctl(StringView program_name, ByteBuffer& commarea);
    
    // RETURN - Return from program
    Result<void> return_program();
    Result<void> return_program(Int32 response);
    Result<void> return_transid(const FixedString<4>& transid);
    Result<void> return_transid(const FixedString<4>& transid, void* commarea, UInt32 length);
    
    // LOAD - Load program into storage
    Result<void*> load(StringView program_name);
    Result<void*> load(StringView program_name, bool hold);
    
    // RELEASE - Release loaded program
    Result<void> release(StringView program_name);
    Result<void> release_all();
    
    // Query operations
    [[nodiscard]] FixedString<8> get_current_program() const;
    [[nodiscard]] UInt32 get_link_depth() const;
    [[nodiscard]] std::vector<LinkLevel> get_link_stack() const;
    
    // Enable/Disable programs
    Result<void> enable_program(StringView name);
    Result<void> disable_program(StringView name);
    Result<void> newcopy_program(StringView name);
    
    // Statistics
    [[nodiscard]] String get_statistics() const;
    void reset_statistics();
};

// =============================================================================
// EXEC CICS Style Interface
// =============================================================================

Result<Int32> exec_cics_link(StringView program);
Result<Int32> exec_cics_link(StringView program, void* commarea, UInt32 length);
Result<void> exec_cics_xctl(StringView program);
Result<void> exec_cics_xctl(StringView program, void* commarea, UInt32 length);
Result<void> exec_cics_return();
Result<void> exec_cics_return_transid(StringView transid);
Result<void*> exec_cics_load(StringView program);
Result<void> exec_cics_release(StringView program);

// =============================================================================
// Program Registration Helper
// =============================================================================

class ProgramRegistrar {
public:
    ProgramRegistrar(StringView name, ProgramFunction func);
    ~ProgramRegistrar() = default;
};

// Macro for easy program registration
#define CICS_REGISTER_PROGRAM(name, func) \
    static cics::program::ProgramRegistrar __program_##name##_registrar(#name, func)

} // namespace program
} // namespace cics

#endif // CICS_PROGRAM_CONTROL_HPP
