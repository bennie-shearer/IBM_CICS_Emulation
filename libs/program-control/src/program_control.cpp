// =============================================================================
// CICS Emulation - Program Control Module Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/program/program_control.hpp>
#include <sstream>
#include <algorithm>

namespace cics {
namespace program {

// Thread-local storage
thread_local std::stack<LinkLevel> ProgramControlManager::link_stack_;
thread_local FixedString<8> ProgramControlManager::current_program_;

// =============================================================================
// ProgramDefinition Implementation
// =============================================================================

String ProgramDefinition::to_string() const {
    std::ostringstream oss;
    oss << "Program{name=" << String(program_name.data(), 8)
        << ", type=" << static_cast<int>(type)
        << ", status=" << static_cast<int>(status)
        << ", use_count=" << use_count
        << "}";
    return oss.str();
}

// =============================================================================
// ProgramControlManager Implementation
// =============================================================================

ProgramControlManager& ProgramControlManager::instance() {
    static ProgramControlManager instance;
    return instance;
}

Result<void> ProgramControlManager::define_program(const ProgramDefinition& def) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    String name(def.program_name.data(), 8);
    // Trim trailing spaces
    name.erase(std::find_if(name.rbegin(), name.rend(), 
        [](unsigned char ch) { return !std::isspace(ch); }).base(), name.end());
    
    programs_[name] = def;
    programs_[name].status = ProgramStatus::ENABLED;
    
    return make_success();
}

Result<void> ProgramControlManager::define_program(StringView name, ProgramFunction entry_point) {
    ProgramDefinition def;
    def.program_name = name;
    def.type = ProgramType::NATIVE;
    def.entry_point = std::move(entry_point);
    def.status = ProgramStatus::ENABLED;
    
    return define_program(def);
}

Result<void> ProgramControlManager::undefine_program(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    String key(name);
    auto it = programs_.find(key);
    if (it == programs_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Program not found: " + key);
    }
    
    if (it->second.use_count > 0) {
        return make_error<void>(ErrorCode::RESOURCE_EXHAUSTED,
            "Program is in use: " + key);
    }
    
    programs_.erase(it);
    return make_success();
}

Result<ProgramDefinition*> ProgramControlManager::get_program(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    String key(name);
    auto it = programs_.find(key);
    if (it == programs_.end()) {
        return make_error<ProgramDefinition*>(ErrorCode::RECORD_NOT_FOUND,
            "Program not found: " + key);
    }
    
    return make_success(&it->second);
}

bool ProgramControlManager::program_exists(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return programs_.find(String(name)) != programs_.end();
}

std::vector<String> ProgramControlManager::list_programs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<String> result;
    for (const auto& [name, def] : programs_) {
        result.push_back(name);
    }
    return result;
}

Result<Int32> ProgramControlManager::link(StringView program_name) {
    return link(program_name, nullptr, 0);
}

Result<Int32> ProgramControlManager::link(StringView program_name, void* commarea, 
                                           UInt32 commarea_length) {
    ProgramDefinition* program = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.link_count;
        
        String key(program_name);
        auto it = programs_.find(key);
        if (it == programs_.end()) {
            ++stats_.program_not_found;
            return make_error<Int32>(ErrorCode::RECORD_NOT_FOUND,
                "Program not found: " + key);
        }
        
        program = &it->second;
        
        if (program->status == ProgramStatus::DISABLED) {
            return make_error<Int32>(ErrorCode::INVALID_STATE,
                "Program is disabled: " + key);
        }
        
        if (!program->entry_point) {
            return make_error<Int32>(ErrorCode::INVALID_STATE,
                "Program has no entry point: " + key);
        }
        
        ++program->use_count;
    }
    
    // Push current level
    LinkLevel level;
    level.program_name = current_program_;
    level.commarea = commarea;
    level.commarea_length = commarea_length;
    level.entry_time = std::chrono::steady_clock::now();
    link_stack_.push(level);
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.max_link_depth = std::max(stats_.max_link_depth, 
            static_cast<UInt32>(link_stack_.size()));
    }
    
    // Save current and switch
    FixedString<8> saved_program = current_program_;
    current_program_ = program->program_name;
    
    // Execute the program
    Int32 result = 0;
    try {
        result = program->entry_point(commarea, commarea_length);
    } catch (...) {
        // Restore state on exception
        current_program_ = saved_program;
        link_stack_.pop();
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --program->use_count;
        }
        throw;
    }
    
    // Restore state
    current_program_ = saved_program;
    link_stack_.pop();
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        --program->use_count;
    }
    
    return make_success(result);
}

Result<Int32> ProgramControlManager::link(StringView program_name, ByteBuffer& commarea) {
    return link(program_name, commarea.data(), static_cast<UInt32>(commarea.size()));
}

Result<void> ProgramControlManager::xctl(StringView program_name) {
    return xctl(program_name, nullptr, 0);
}

Result<void> ProgramControlManager::xctl(StringView program_name, void* commarea,
                                          UInt32 commarea_length) {
    ProgramDefinition* program = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.xctl_count;
        
        String key(program_name);
        auto it = programs_.find(key);
        if (it == programs_.end()) {
            ++stats_.program_not_found;
            return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
                "Program not found: " + key);
        }
        
        program = &it->second;
        
        if (program->status == ProgramStatus::DISABLED) {
            return make_error<void>(ErrorCode::INVALID_STATE,
                "Program is disabled: " + key);
        }
        
        if (!program->entry_point) {
            return make_error<void>(ErrorCode::INVALID_STATE,
                "Program has no entry point: " + key);
        }
        
        ++program->use_count;
    }
    
    // Replace current link level (don't push)
    current_program_ = program->program_name;
    
    // Execute the program
    try {
        program->entry_point(commarea, commarea_length);
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --program->use_count;
        }
        throw;
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        --program->use_count;
    }
    
    return make_success();
}

Result<void> ProgramControlManager::xctl(StringView program_name, ByteBuffer& commarea) {
    return xctl(program_name, commarea.data(), static_cast<UInt32>(commarea.size()));
}

Result<void> ProgramControlManager::return_program() {
    return return_program(0);
}

Result<void> ProgramControlManager::return_program(Int32 response) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.return_count;
    
    if (!link_stack_.empty()) {
        link_stack_.top().response_code = response;
    }
    
    return make_success();
}

Result<void> ProgramControlManager::return_transid(const FixedString<4>& transid) {
    return return_transid(transid, nullptr, 0);
}

Result<void> ProgramControlManager::return_transid(const FixedString<4>& transid,
                                                    void* commarea, UInt32 length) {
    (void)transid;   // Reserved for future CICS transaction manager integration
    (void)commarea;  // Reserved for future CICS transaction manager integration
    (void)length;    // Reserved for future CICS transaction manager integration
    
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.return_count;
    
    // Store transid for next transaction (would typically involve CICS transaction manager)
    // For now, just acknowledge the return
    
    return make_success();
}

Result<void*> ProgramControlManager::load(StringView program_name) {
    return load(program_name, false);
}

Result<void*> ProgramControlManager::load(StringView program_name, bool hold) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.load_count;
    
    String key(program_name);
    auto it = programs_.find(key);
    if (it == programs_.end()) {
        ++stats_.program_not_found;
        return make_error<void*>(ErrorCode::RECORD_NOT_FOUND,
            "Program not found: " + key);
    }
    
    ProgramDefinition& program = it->second;
    program.status = ProgramStatus::LOADED;
    ++program.load_count;
    program.load_time = std::chrono::steady_clock::now();
    
    if (hold) {
        program.resident = true;
    }
    
    // Return address of entry point (or load address for real programs)
    return make_success(program.load_address);
}

Result<void> ProgramControlManager::release(StringView program_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.release_count;
    
    String key(program_name);
    auto it = programs_.find(key);
    if (it == programs_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Program not found: " + key);
    }
    
    ProgramDefinition& program = it->second;
    
    if (program.use_count > 0) {
        return make_error<void>(ErrorCode::RESOURCE_EXHAUSTED,
            "Program is in use: " + key);
    }
    
    if (!program.resident) {
        program.status = ProgramStatus::NOT_LOADED;
        if (program.load_count > 0) {
            --program.load_count;
        }
    }
    
    return make_success();
}

Result<void> ProgramControlManager::release_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [name, program] : programs_) {
        if (program.use_count == 0 && !program.resident) {
            program.status = ProgramStatus::NOT_LOADED;
            program.load_count = 0;
        }
    }
    
    return make_success();
}

FixedString<8> ProgramControlManager::get_current_program() const {
    return current_program_;
}

UInt32 ProgramControlManager::get_link_depth() const {
    return static_cast<UInt32>(link_stack_.size());
}

std::vector<LinkLevel> ProgramControlManager::get_link_stack() const {
    std::vector<LinkLevel> result;
    std::stack<LinkLevel> temp = link_stack_;
    while (!temp.empty()) {
        result.push_back(temp.top());
        temp.pop();
    }
    std::reverse(result.begin(), result.end());
    return result;
}

Result<void> ProgramControlManager::enable_program(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    String key(name);
    auto it = programs_.find(key);
    if (it == programs_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Program not found: " + key);
    }
    
    it->second.status = ProgramStatus::ENABLED;
    return make_success();
}

Result<void> ProgramControlManager::disable_program(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    String key(name);
    auto it = programs_.find(key);
    if (it == programs_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Program not found: " + key);
    }
    
    if (it->second.use_count > 0) {
        return make_error<void>(ErrorCode::RESOURCE_EXHAUSTED,
            "Program is in use: " + key);
    }
    
    it->second.status = ProgramStatus::DISABLED;
    return make_success();
}

Result<void> ProgramControlManager::newcopy_program(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    String key(name);
    auto it = programs_.find(key);
    if (it == programs_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Program not found: " + key);
    }
    
    it->second.status = ProgramStatus::NEW_COPY;
    return make_success();
}

String ProgramControlManager::get_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "Program Control Statistics:\n"
        << "  LINK calls:           " << stats_.link_count << "\n"
        << "  XCTL calls:           " << stats_.xctl_count << "\n"
        << "  RETURN calls:         " << stats_.return_count << "\n"
        << "  LOAD calls:           " << stats_.load_count << "\n"
        << "  RELEASE calls:        " << stats_.release_count << "\n"
        << "  Program not found:    " << stats_.program_not_found << "\n"
        << "  Max link depth:       " << stats_.max_link_depth << "\n"
        << "  Defined programs:     " << programs_.size() << "\n";
    
    return oss.str();
}

void ProgramControlManager::reset_statistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Statistics{};
}

// =============================================================================
// EXEC CICS Interface Implementation
// =============================================================================

Result<Int32> exec_cics_link(StringView program) {
    return ProgramControlManager::instance().link(program);
}

Result<Int32> exec_cics_link(StringView program, void* commarea, UInt32 length) {
    return ProgramControlManager::instance().link(program, commarea, length);
}

Result<void> exec_cics_xctl(StringView program) {
    return ProgramControlManager::instance().xctl(program);
}

Result<void> exec_cics_xctl(StringView program, void* commarea, UInt32 length) {
    return ProgramControlManager::instance().xctl(program, commarea, length);
}

Result<void> exec_cics_return() {
    return ProgramControlManager::instance().return_program();
}

Result<void> exec_cics_return_transid(StringView transid) {
    FixedString<4> trans;
    trans = transid;
    return ProgramControlManager::instance().return_transid(trans);
}

Result<void*> exec_cics_load(StringView program) {
    return ProgramControlManager::instance().load(program);
}

Result<void> exec_cics_release(StringView program) {
    return ProgramControlManager::instance().release(program);
}

// =============================================================================
// ProgramRegistrar Implementation
// =============================================================================

ProgramRegistrar::ProgramRegistrar(StringView name, ProgramFunction func) {
    ProgramControlManager::instance().define_program(name, std::move(func));
}

} // namespace program
} // namespace cics
