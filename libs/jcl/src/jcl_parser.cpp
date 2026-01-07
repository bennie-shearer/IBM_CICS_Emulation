// =============================================================================
// CICS Emulation - JCL Parser Implementation
// Version: 3.4.6
// =============================================================================

#include "cics/jcl/jcl_parser.hpp"
#include <algorithm>
#include <format>
#include <sstream>
#include <fstream>
#include <cctype>

namespace cics::jcl {

// =============================================================================
// Disposition Implementation
// =============================================================================

String Disposition::to_string() const {
    std::ostringstream oss;
    oss << "DISP=(";
    
    switch (status) {
        case DatasetStatus::NEW: oss << "NEW"; break;
        case DatasetStatus::OLD: oss << "OLD"; break;
        case DatasetStatus::SHR: oss << "SHR"; break;
        case DatasetStatus::MOD: oss << "MOD"; break;
    }
    
    oss << ",";
    
    switch (normal) {
        case NormalDisposition::DELETE: oss << "DELETE"; break;
        case NormalDisposition::KEEP: oss << "KEEP"; break;
        case NormalDisposition::PASS: oss << "PASS"; break;
        case NormalDisposition::CATLG: oss << "CATLG"; break;
        case NormalDisposition::UNCATLG: oss << "UNCATLG"; break;
    }
    
    oss << ",";
    
    switch (abnormal) {
        case AbnormalDisposition::DELETE: oss << "DELETE"; break;
        case AbnormalDisposition::KEEP: oss << "KEEP"; break;
        case AbnormalDisposition::CATLG: oss << "CATLG"; break;
        case AbnormalDisposition::UNCATLG: oss << "UNCATLG"; break;
    }
    
    oss << ")";
    return oss.str();
}

Result<Disposition> Disposition::parse(StringView str) {
    Disposition disp;
    
    // Remove parentheses if present
    String s(str);
    if (!s.empty() && s.front() == '(') s = s.substr(1);
    if (!s.empty() && s.back() == ')') s.pop_back();
    
    auto parts = split(s, ',');
    
    if (parts.empty()) {
        return make_error<Disposition>(ErrorCode::INVALID_ARGUMENT, "Empty disposition");
    }
    
    // Parse status
    String status_str = to_upper(trim(parts[0]));
    if (status_str == "NEW") disp.status = DatasetStatus::NEW;
    else if (status_str == "OLD") disp.status = DatasetStatus::OLD;
    else if (status_str == "SHR") disp.status = DatasetStatus::SHR;
    else if (status_str == "MOD") disp.status = DatasetStatus::MOD;
    else {
        return make_error<Disposition>(ErrorCode::INVALID_ARGUMENT,
            std::format("Invalid disposition status: {}", status_str));
    }
    
    // Parse normal disposition
    if (parts.size() > 1 && !parts[1].empty()) {
        String norm_str = to_upper(trim(parts[1]));
        if (norm_str == "DELETE") disp.normal = NormalDisposition::DELETE;
        else if (norm_str == "KEEP") disp.normal = NormalDisposition::KEEP;
        else if (norm_str == "PASS") disp.normal = NormalDisposition::PASS;
        else if (norm_str == "CATLG") disp.normal = NormalDisposition::CATLG;
        else if (norm_str == "UNCATLG") disp.normal = NormalDisposition::UNCATLG;
    }
    
    // Parse abnormal disposition
    if (parts.size() > 2 && !parts[2].empty()) {
        String abnorm_str = to_upper(trim(parts[2]));
        if (abnorm_str == "DELETE") disp.abnormal = AbnormalDisposition::DELETE;
        else if (abnorm_str == "KEEP") disp.abnormal = AbnormalDisposition::KEEP;
        else if (abnorm_str == "CATLG") disp.abnormal = AbnormalDisposition::CATLG;
        else if (abnorm_str == "UNCATLG") disp.abnormal = AbnormalDisposition::UNCATLG;
    }
    
    return disp;
}

// =============================================================================
// SpaceAllocation Implementation
// =============================================================================

String SpaceAllocation::to_string() const {
    std::ostringstream oss;
    oss << "SPACE=(";
    
    switch (unit) {
        case SpaceUnit::TRACKS: oss << "TRK"; break;
        case SpaceUnit::CYLINDERS: oss << "CYL"; break;
        case SpaceUnit::BLOCKS: oss << block_size; break;
        case SpaceUnit::BYTES: oss << "BYTES"; break;
        case SpaceUnit::KILOBYTES: oss << "KB"; break;
        case SpaceUnit::MEGABYTES: oss << "MB"; break;
        case SpaceUnit::RECORDS: oss << "RECORDS"; break;
    }
    
    oss << ",(" << primary;
    if (secondary > 0) oss << "," << secondary;
    if (directory > 0) oss << "," << directory;
    oss << ")";
    
    if (rlse) oss << ",RLSE";
    if (contig) oss << ",CONTIG";
    if (round) oss << ",ROUND";
    
    oss << ")";
    return oss.str();
}

// =============================================================================
// DCBParameters Implementation
// =============================================================================

String DCBParameters::to_string() const {
    std::ostringstream oss;
    oss << "DCB=(";
    bool first = true;
    
    if (recfm.has_value()) {
        if (!first) oss << ",";
        oss << "RECFM=";
        switch (recfm.value()) {
            case RecordFormat::F: oss << "F"; break;
            case RecordFormat::FB: oss << "FB"; break;
            case RecordFormat::V: oss << "V"; break;
            case RecordFormat::VB: oss << "VB"; break;
            case RecordFormat::U: oss << "U"; break;
            case RecordFormat::FBA: oss << "FBA"; break;
            case RecordFormat::VBA: oss << "VBA"; break;
            case RecordFormat::FM: oss << "FM"; break;
            case RecordFormat::VM: oss << "VM"; break;
        }
        first = false;
    }
    
    if (lrecl.has_value()) {
        if (!first) oss << ",";
        oss << "LRECL=" << lrecl.value();
        first = false;
    }
    
    if (blksize.has_value()) {
        if (!first) oss << ",";
        oss << "BLKSIZE=" << blksize.value();
        first = false;
    }
    
    if (dsorg.has_value()) {
        if (!first) oss << ",";
        oss << "DSORG=";
        switch (dsorg.value()) {
            case DatasetOrg::PS: oss << "PS"; break;
            case DatasetOrg::PO: oss << "PO"; break;
            case DatasetOrg::DA: oss << "DA"; break;
            case DatasetOrg::IS: oss << "IS"; break;
            case DatasetOrg::VS: oss << "VS"; break;
        }
        first = false;
    }
    
    oss << ")";
    return oss.str();
}

// =============================================================================
// DDParameters Implementation
// =============================================================================

String DDParameters::to_string() const {
    std::ostringstream oss;
    
    if (dummy) {
        oss << "DUMMY";
        return oss.str();
    }
    
    if (!dsn.empty()) {
        oss << "DSN=" << dsn;
        if (!member.empty()) oss << "(" << member << ")";
    }
    
    if (disp.has_value()) {
        if (!oss.str().empty()) oss << ",";
        oss << disp.value().to_string();
    }
    
    if (!sysout.empty()) {
        if (!oss.str().empty()) oss << ",";
        oss << "SYSOUT=" << sysout;
    }
    
    return oss.str();
}

// =============================================================================
// ExecParameters Implementation
// =============================================================================

String ExecParameters::to_string() const {
    std::ostringstream oss;
    
    if (!pgm.empty()) {
        oss << "PGM=" << pgm;
    } else if (!proc.empty()) {
        oss << proc;
    }
    
    if (!parm.empty()) {
        oss << ",PARM='" << parm << "'";
    }
    
    if (!cond.empty()) {
        oss << ",COND=" << cond;
    }
    
    if (!region.empty()) {
        oss << ",REGION=" << region;
    }
    
    if (!time.empty()) {
        oss << ",TIME=" << time;
    }
    
    return oss.str();
}

// =============================================================================
// JobParameters Implementation
// =============================================================================

String JobParameters::to_string() const {
    std::ostringstream oss;
    
    if (!account.empty()) {
        oss << "(" << account << ")";
    }
    
    if (!programmer.empty()) {
        oss << ",'" << programmer << "'";
    }
    
    if (!class_name.empty()) {
        oss << ",CLASS=" << class_name;
    }
    
    if (!msgclass.empty()) {
        oss << ",MSGCLASS=" << msgclass;
    }
    
    if (!msglevel.empty()) {
        oss << ",MSGLEVEL=" << msglevel;
    }
    
    if (!notify.empty()) {
        oss << ",NOTIFY=" << notify;
    }
    
    if (!region.empty()) {
        oss << ",REGION=" << region;
    }
    
    if (!time.empty()) {
        oss << ",TIME=" << time;
    }
    
    return oss.str();
}

// =============================================================================
// JCLStatement Implementation
// =============================================================================

String JCLStatement::to_string() const {
    std::ostringstream oss;
    oss << "//" << name;
    if (!name.empty()) oss << " ";
    oss << operation;
    if (!operands.empty()) oss << " " << operands;
    if (!comment.empty()) oss << " " << comment;
    return oss.str();
}

// =============================================================================
// JCLStep Implementation
// =============================================================================

Optional<const DDParameters*> JCLStep::get_dd(StringView ddname) const {
    String upper_name = to_upper(String(ddname));
    for (const auto& [name, dd] : dd_statements) {
        if (to_upper(name) == upper_name) {
            return &dd;
        }
    }
    return std::nullopt;
}

String JCLStep::to_string() const {
    std::ostringstream oss;
    oss << "//" << step_name << " EXEC " << exec.to_string() << "\n";
    for (const auto& [name, dd] : dd_statements) {
        oss << "//" << name << " DD " << dd.to_string() << "\n";
    }
    return oss.str();
}

// =============================================================================
// JCLJob Implementation
// =============================================================================

Optional<const JCLStep*> JCLJob::get_step(StringView step_name) const {
    String upper_name = to_upper(String(step_name));
    for (const auto& step : steps) {
        if (to_upper(step.step_name) == upper_name) {
            return &step;
        }
    }
    return std::nullopt;
}

Optional<const JCLStep*> JCLJob::get_step(UInt32 step_number) const {
    if (step_number > 0 && step_number <= steps.size()) {
        return &steps[step_number - 1];
    }
    return std::nullopt;
}

String JCLJob::to_string() const {
    std::ostringstream oss;
    oss << "//" << job_params.job_name << " JOB " << job_params.to_string() << "\n";
    for (const auto& step : steps) {
        oss << step.to_string();
    }
    return oss.str();
}

String JCLJob::to_json() const {
    std::ostringstream oss;
    oss << "{\"job_name\":\"" << job_params.job_name << "\","
        << "\"steps\":[";
    bool first = true;
    for (const auto& step : steps) {
        if (!first) oss << ",";
        oss << "{\"step_name\":\"" << step.step_name << "\","
            << "\"pgm\":\"" << step.exec.pgm << "\"}";
        first = false;
    }
    oss << "]}";
    return oss.str();
}

// =============================================================================
// ParseError Implementation
// =============================================================================

String ParseError::to_string() const {
    return std::format("{}:{}: {} - {} [{}]",
        line, column, cics::jcl::to_string(code), message, context);
}

// =============================================================================
// JCLParser Implementation
// =============================================================================

JCLParser::JCLParser(ParserOptions options)
    : options_(std::move(options)) {
    // Add default symbols
    for (const auto& [name, value] : options_.default_symbols) {
        symbols_[to_upper(name)] = value;
    }
}

void JCLParser::reset() {
    errors_.clear();
    warnings_.clear();
    symbols_.clear();
    current_line_ = 0;
    
    for (const auto& [name, value] : options_.default_symbols) {
        symbols_[to_upper(name)] = value;
    }
}

void JCLParser::add_error(JCLError code, StringView message, StringView context) {
    errors_.push_back({code, current_line_, 0, String(message), String(context)});
}

void JCLParser::add_warning(JCLError code, StringView message, StringView context) {
    warnings_.push_back({code, current_line_, 0, String(message), String(context)});
}

bool JCLParser::is_valid_label(StringView label) const {
    if (label.empty() || label.length() > 8) return false;
    if (!std::isalpha(static_cast<unsigned char>(label[0])) && label[0] != '@' && 
        label[0] != '#' && label[0] != '$') return false;
    
    for (char c : label) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && 
            c != '@' && c != '#' && c != '$') return false;
    }
    return true;
}

bool JCLParser::is_valid_dsname(StringView dsn) const {
    if (dsn.empty() || dsn.length() > 44) return false;
    
    auto qualifiers = split(String(dsn), '.');
    if (qualifiers.empty() || qualifiers.size() > 22) return false;
    
    for (const auto& qual : qualifiers) {
        if (qual.empty() || qual.length() > 8) return false;
        if (!std::isalpha(static_cast<unsigned char>(qual[0])) &&
            qual[0] != '@' && qual[0] != '#' && qual[0] != '$') return false;
    }
    
    return true;
}

String JCLParser::substitute_symbols(StringView str) {
    if (!options_.resolve_symbols) return String(str);
    
    String result(str);
    Size pos = 0;
    
    while ((pos = result.find('&', pos)) != String::npos) {
        // Find end of symbol name
        Size end = pos + 1;
        while (end < result.length() && 
               (std::isalnum(static_cast<unsigned char>(result[end])) || result[end] == '@' || 
                result[end] == '#' || result[end] == '$')) {
            ++end;
        }
        
        if (end > pos + 1) {
            String sym_name = result.substr(pos + 1, end - pos - 1);
            
            // Check for double ampersand (escaped)
            if (pos > 0 && result[pos - 1] == '&') {
                pos = end;
                continue;
            }
            
            // Check for trailing dot (system symbol)
            bool has_dot = (end < result.length() && result[end] == '.');
            
            auto sym_it = symbols_.find(to_upper(sym_name));
            if (sym_it != symbols_.end()) {
                Size replace_len = end - pos + (has_dot ? 1 : 0);
                result.replace(pos, replace_len, sym_it->second);
                pos += sym_it->second.length();
            } else {
                if (options_.strict_mode) {
                    add_warning(JCLError::UNDEFINED_SYMBOL,
                        std::format("Undefined symbol: &{}", sym_name));
                }
                pos = end;
            }
        } else {
            ++pos;
        }
    }
    
    return result;
}

void JCLParser::set_symbol(StringView name, StringView value) {
    symbols_[to_upper(String(name))] = String(value);
}

Optional<String> JCLParser::get_symbol(StringView name) const {
    auto it = symbols_.find(to_upper(String(name)));
    if (it != symbols_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void JCLParser::add_symbol(StringView name, StringView value) {
    set_symbol(name, value);
}

void JCLParser::clear_symbols() {
    symbols_.clear();
    for (const auto& [name, value] : options_.default_symbols) {
        symbols_[to_upper(name)] = value;
    }
}

std::pair<String, String> JCLParser::split_keyword_value(StringView param) {
    Size eq_pos = param.find('=');
    if (eq_pos == StringView::npos) {
        return {String(param), ""};
    }
    return {String(param.substr(0, eq_pos)), String(param.substr(eq_pos + 1))};
}

Result<StatementType> JCLParser::identify_statement(StringView operation) {
    String op = to_upper(String(operation));
    
    if (op == "JOB") return StatementType::JOB;
    if (op == "EXEC") return StatementType::EXEC;
    if (op == "DD") return StatementType::DD;
    if (op == "PROC") return StatementType::PROC;
    if (op == "PEND") return StatementType::PEND;
    if (op == "SET") return StatementType::SET;
    if (op == "IF") return StatementType::IF;
    if (op == "ELSE") return StatementType::ELSE;
    if (op == "ENDIF") return StatementType::ENDIF;
    if (op == "INCLUDE") return StatementType::INCLUDE;
    if (op == "JCLLIB") return StatementType::JCLLIB;
    if (op == "OUTPUT") return StatementType::OUTPUT;
    
    return StatementType::UNKNOWN;
}

Result<JCLStatement> JCLParser::parse_statement(StringView line) {
    JCLStatement stmt;
    stmt.line_number = current_line_;
    
    // Check for comment
    if (line.starts_with("//*")) {
        stmt.type = StatementType::COMMENT;
        stmt.comment = String(line.substr(3));
        return stmt;
    }
    
    // Check for null statement
    if (line == "//") {
        stmt.type = StatementType::NULL_STATEMENT;
        return stmt;
    }
    
    // Check for delimiter statement
    if (line.starts_with("/*")) {
        stmt.type = StatementType::DELIMITER;
        return stmt;
    }
    
    // Must start with //
    if (!line.starts_with("//")) {
        // Could be continuation or instream data
        return stmt;
    }
    
    // Parse the statement
    StringView remaining = line.substr(2);
    
    // Extract name (if present)
    Size pos = 0;
    while (pos < remaining.length() && !std::isspace(static_cast<unsigned char>(remaining[pos]))) {
        ++pos;
    }
    
    if (pos > 0) {
        StringView first_field = remaining.substr(0, pos);
        
        // Check if this is a name or operation
        auto type_result = identify_statement(first_field);
        if (type_result.is_success() && type_result.value() != StatementType::UNKNOWN) {
            // First field is operation
            stmt.operation = String(first_field);
            stmt.type = type_result.value();
            remaining = remaining.substr(pos);
        } else {
            // First field is name
            stmt.name = String(first_field);
            remaining = remaining.substr(pos);
            
            // Skip whitespace
            while (!remaining.empty() && std::isspace(static_cast<unsigned char>(remaining[0]))) {
                remaining = remaining.substr(1);
            }
            
            // Get operation
            pos = 0;
            while (pos < remaining.length() && !std::isspace(static_cast<unsigned char>(remaining[pos]))) {
                ++pos;
            }
            
            if (pos > 0) {
                stmt.operation = String(remaining.substr(0, pos));
                auto type_result2 = identify_statement(stmt.operation);
                stmt.type = type_result2.is_success() ? type_result2.value() : StatementType::UNKNOWN;
                remaining = remaining.substr(pos);
            }
        }
    }
    
    // Skip whitespace
    while (!remaining.empty() && std::isspace(static_cast<unsigned char>(remaining[0]))) {
        remaining = remaining.substr(1);
    }
    
    // Rest is operands (and possibly comments)
    stmt.operands = String(remaining);
    
    return stmt;
}

Result<JCLJob> JCLParser::parse(StringView jcl) {
    reset();
    
    JCLJob job;
    std::vector<JCLStatement> statements;
    
    // Split into lines
    auto lines = split(String(jcl), '\n');
    
    JCLStep* current_step = nullptr;
    bool in_job = false;
    String continuation_buffer;
    
    for (Size i = 0; i < lines.size(); ++i) {
        current_line_ = static_cast<UInt32>(i + 1);
        String line = lines[i];
        
        // Remove trailing carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Handle continuation
        if (!continuation_buffer.empty()) {
            // Check if this is a continuation line
            if (line.starts_with("//") && line.length() > 2 && 
                std::isspace(static_cast<unsigned char>(line[2]))) {
                // Continuation - append after trimming prefix
                Size start = 3;
                while (start < line.length() && std::isspace(static_cast<unsigned char>(line[start]))) {
                    ++start;
                }
                continuation_buffer += line.substr(start);
                continue;
            } else {
                // Not a continuation - process buffered statement
                // (Parse would happen here with the buffer)
                continuation_buffer.clear();
            }
        }
        
        auto stmt_result = parse_statement(line);
        if (!stmt_result) {
            add_error(JCLError::SYNTAX_ERROR, stmt_result.error().message, line);
            continue;
        }
        
        JCLStatement stmt = stmt_result.value();
        stmt.operands = substitute_symbols(stmt.operands);
        statements.push_back(stmt);
        
        switch (stmt.type) {
            case StatementType::JOB: {
                if (in_job) {
                    add_warning(JCLError::SYNTAX_ERROR, "Multiple JOB statements");
                }
                in_job = true;
                job.job_params.job_name = stmt.name;
                // Parse job parameters from operands
                // Simplified parsing - real implementation would be more complex
                break;
            }
            
            case StatementType::EXEC: {
                if (!in_job) {
                    add_error(JCLError::MISSING_JOB, "EXEC without JOB statement");
                    continue;
                }
                
                JCLStep step;
                step.step_name = stmt.name;
                step.step_number = static_cast<UInt32>(job.steps.size() + 1);
                
                // Parse exec parameters
                String operands = stmt.operands;
                if (operands.starts_with("PGM=")) {
                    Size end = operands.find(',');
                    step.exec.pgm = operands.substr(4, end == String::npos ? end : end - 4);
                } else {
                    // Could be a proc call
                    Size end = operands.find(',');
                    step.exec.proc = operands.substr(0, end == String::npos ? operands.length() : end);
                }
                
                job.steps.push_back(std::move(step));
                current_step = &job.steps.back();
                break;
            }
            
            case StatementType::DD: {
                if (!current_step) {
                    add_error(JCLError::MISSING_EXEC, "DD without EXEC statement");
                    continue;
                }
                
                DDParameters dd;
                String operands = stmt.operands;
                
                // Simple parsing of common DD parameters
                if (operands == "DUMMY") {
                    dd.dummy = true;
                } else if (operands.starts_with("DSN=") || operands.starts_with("DSNAME=")) {
                    Size start = operands.find('=') + 1;
                    Size end = operands.find(',', start);
                    String dsn_str = operands.substr(start, end == String::npos ? end : end - start);
                    
                    // Check for member
                    Size paren = dsn_str.find('(');
                    if (paren != String::npos) {
                        dd.member = dsn_str.substr(paren + 1, dsn_str.length() - paren - 2);
                        dd.dsn = dsn_str.substr(0, paren);
                    } else {
                        dd.dsn = dsn_str;
                    }
                } else if (operands.starts_with("SYSOUT=")) {
                    dd.sysout = operands.substr(7, 1);
                } else if (operands == "*") {
                    dd.instream = true;
                }
                
                current_step->dd_statements.emplace_back(stmt.name, std::move(dd));
                break;
            }
            
            case StatementType::SET: {
                // Parse SET statement for symbolic assignment
                auto parts = split(stmt.operands, '=');
                if (parts.size() == 2) {
                    set_symbol(trim(parts[0]), trim(parts[1]));
                }
                break;
            }
            
            default:
                break;
        }
    }
    
    job.all_statements = std::move(statements);
    
    if (!in_job && options_.strict_mode) {
        add_error(JCLError::MISSING_JOB, "No JOB statement found");
    }
    
    if (has_errors()) {
        return make_error<JCLJob>(ErrorCode::INVALID_ARGUMENT, 
            std::format("{} error(s) during JCL parsing", errors_.size()));
    }
    
    return job;
}

Result<JCLJob> JCLParser::parse_file(const Path& path) {
    std::ifstream file(path);
    if (!file) {
        return make_error<JCLJob>(ErrorCode::IO_ERROR, 
            std::format("Failed to open file: {}", path.string()));
    }
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    
    return parse(buffer.str());
}

Result<JCLStatement> JCLParser::parse_single_statement(StringView line) {
    current_line_ = 1;
    return parse_statement(line);
}

// =============================================================================
// JCLValidator Implementation
// =============================================================================

Result<void> JCLValidator::validate(const JCLJob& job) {
    reset();
    
    auto job_result = validate_job_params(job.job_params);
    if (!job_result) {
        ParseError err;
        err.code = JCLError::SYNTAX_ERROR;
        err.line = 0;
        err.column = 0;
        err.message = job_result.error().message;
        err.context = "";
        errors_.push_back(std::move(err));
    }
    
    for (const auto& step : job.steps) {
        auto step_result = validate_step(step);
        if (!step_result) {
            ParseError err;
            err.code = JCLError::SYNTAX_ERROR;
            err.line = 0;
            err.column = 0;
            err.message = step_result.error().message;
            err.context = step.step_name;
            errors_.push_back(std::move(err));
        }
    }
    
    if (!errors_.empty()) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT,
            std::format("{} validation error(s)", errors_.size()));
    }
    
    return {};
}

Result<void> JCLValidator::validate_job_params([[maybe_unused]] const JobParameters& params) {
    // Add specific job parameter validations
    return {};
}

Result<void> JCLValidator::validate_exec_params(const ExecParameters& params) {
    if (params.pgm.empty() && params.proc.empty()) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "EXEC must specify PGM or procedure");
    }
    return {};
}

Result<void> JCLValidator::validate_dd_params([[maybe_unused]] const DDParameters& params) {
    // Add specific DD parameter validations
    return {};
}

Result<void> JCLValidator::validate_step(const JCLStep& step) {
    return validate_exec_params(step.exec);
}

Result<void> JCLValidator::validate_dsname(StringView dsn) {
    if (dsn.empty()) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Empty dataset name");
    }
    
    if (dsn.length() > 44) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Dataset name exceeds 44 characters");
    }
    
    return {};
}

void JCLValidator::reset() {
    errors_.clear();
    warnings_.clear();
}

// =============================================================================
// JCLGenerator Implementation
// =============================================================================

void JCLGenerator::reset() {
    output_.clear();
    current_column_ = 0;
}

void JCLGenerator::emit(StringView str) {
    output_ += str;
    current_column_ += static_cast<UInt32>(str.length());
}

void JCLGenerator::emit_line(StringView str) {
    output_ += str;
    output_ += '\n';
    current_column_ = 0;
}

String JCLGenerator::generate(const JCLJob& job) {
    reset();
    
    // JOB statement
    emit_line(std::format("//{} JOB {}", job.job_params.job_name, job.job_params.to_string()));
    
    // Steps
    for (const auto& step : job.steps) {
        emit_line(std::format("//{} EXEC {}", step.step_name, step.exec.to_string()));
        
        for (const auto& [ddname, dd] : step.dd_statements) {
            emit_line(std::format("//{} DD {}", ddname, dd.to_string()));
        }
    }
    
    return output_;
}

String JCLGenerator::generate(const JCLStatement& stmt) {
    return stmt.to_string();
}

String JCLGenerator::generate(const JobParameters& params, StringView job_name) {
    return std::format("//{} JOB {}", job_name, params.to_string());
}

String JCLGenerator::generate(const ExecParameters& params, StringView step_name) {
    return std::format("//{} EXEC {}", step_name, params.to_string());
}

String JCLGenerator::generate(const DDParameters& params, StringView dd_name) {
    return std::format("//{} DD {}", dd_name, params.to_string());
}

// =============================================================================
// Utility Functions Implementation
// =============================================================================

Result<DSNComponents> parse_dsn(StringView dsn) {
    DSNComponents result;
    
    String ds(dsn);
    
    // Check for temporary dataset
    if (ds.starts_with("&&")) {
        result.is_temporary = true;
        ds = ds.substr(2);
    }
    
    // Check for member
    Size paren = ds.find('(');
    if (paren != String::npos) {
        Size close = ds.find(')', paren);
        if (close != String::npos) {
            result.member = ds.substr(paren + 1, close - paren - 1);
            
            // Check for GDG relative generation
            if (!result.member.empty() && (result.member[0] == '+' || result.member[0] == '-' || result.member[0] == '0')) {
                result.is_gdg = true;
                try {
                    result.gdg_generation = std::stoi(result.member);
                } catch (...) {
                    // Not a numeric generation
                }
            }
        }
        ds = ds.substr(0, paren);
    }
    
    // Split into qualifiers
    result.qualifiers = split(ds, '.');
    if (!result.qualifiers.empty()) {
        result.high_level_qualifier = result.qualifiers[0];
    }
    
    return result;
}

String DSNComponents::full_dsn() const {
    String result = is_temporary ? "&&" : "";
    result += join(qualifiers, ".");
    if (!member.empty()) {
        result += "(" + member + ")";
    }
    return result;
}

bool is_valid_dsn(StringView dsn) {
    auto result = parse_dsn(dsn);
    if (!result) return false;
    
    const auto& components = result.value();
    
    // Check total length
    String full = join(components.qualifiers, ".");
    if (full.length() > 44) return false;
    
    // Check each qualifier
    for (const auto& qual : components.qualifiers) {
        if (qual.empty() || qual.length() > 8) return false;
        
        // First character must be alphabetic or national
        char first = qual[0];
        if (!std::isalpha(static_cast<unsigned char>(first)) && 
            first != '@' && first != '#' && first != '$') return false;
        
        // Remaining characters must be alphanumeric or national
        for (Size i = 1; i < qual.length(); ++i) {
            char c = qual[i];
            if (!std::isalnum(static_cast<unsigned char>(c)) && 
                c != '@' && c != '#' && c != '$') return false;
        }
    }
    
    return true;
}

bool is_valid_member_name(StringView member) {
    if (member.empty() || member.length() > 8) return false;
    
    char first = member[0];
    if (!std::isalpha(static_cast<unsigned char>(first)) && 
        first != '@' && first != '#' && first != '$') return false;
    
    for (char c : member) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && 
            c != '@' && c != '#' && c != '$') return false;
    }
    
    return true;
}

String normalize_dsn(StringView dsn) {
    return to_upper(String(dsn));
}

} // namespace cics::jcl
