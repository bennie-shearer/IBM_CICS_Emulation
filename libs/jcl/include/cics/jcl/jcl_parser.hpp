#pragma once

// =============================================================================
// CICS Emulation - JCL (Job Control Language) Parser
// Version: 3.4.6
// =============================================================================
//
// IBM JCL parsing and interpretation for:
// - JOB, EXEC, DD statement processing
// - Symbolic parameter substitution
// - Procedure (PROC) expansion
// - Dataset name resolution
// - Disposition and space parameter handling
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <map>
#include <vector>
#include <variant>
#include <regex>

namespace cics::jcl {

using namespace cics;

// =============================================================================
// JCL Statement Types
// =============================================================================

enum class StatementType : UInt8 {
    JOB = 1,
    EXEC = 2,
    DD = 3,
    PROC = 4,
    PEND = 5,
    SET = 6,
    IF = 7,
    ELSE = 8,
    ENDIF = 9,
    INCLUDE = 10,
    JCLLIB = 11,
    OUTPUT = 12,
    COMMENT = 13,
    COMMAND = 14,
    NULL_STATEMENT = 15,
    DELIMITER = 16,
    UNKNOWN = 255
};

[[nodiscard]] constexpr StringView to_string(StatementType type) {
    switch (type) {
        case StatementType::JOB: return "JOB";
        case StatementType::EXEC: return "EXEC";
        case StatementType::DD: return "DD";
        case StatementType::PROC: return "PROC";
        case StatementType::PEND: return "PEND";
        case StatementType::SET: return "SET";
        case StatementType::IF: return "IF";
        case StatementType::ELSE: return "ELSE";
        case StatementType::ENDIF: return "ENDIF";
        case StatementType::INCLUDE: return "INCLUDE";
        case StatementType::JCLLIB: return "JCLLIB";
        case StatementType::OUTPUT: return "OUTPUT";
        case StatementType::COMMENT: return "COMMENT";
        case StatementType::COMMAND: return "COMMAND";
        case StatementType::NULL_STATEMENT: return "NULL";
        case StatementType::DELIMITER: return "DELIMITER";
        case StatementType::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

// =============================================================================
// Dataset Disposition
// =============================================================================

enum class DatasetStatus : UInt8 {
    NEW = 1,
    OLD = 2,
    SHR = 3,
    MOD = 4
};

enum class NormalDisposition : UInt8 {
    DELETE = 1,
    KEEP = 2,
    PASS = 3,
    CATLG = 4,
    UNCATLG = 5
};

enum class AbnormalDisposition : UInt8 {
    DELETE = 1,
    KEEP = 2,
    CATLG = 3,
    UNCATLG = 4
};

struct Disposition {
    DatasetStatus status = DatasetStatus::NEW;
    NormalDisposition normal = NormalDisposition::DELETE;
    AbnormalDisposition abnormal = AbnormalDisposition::DELETE;
    
    [[nodiscard]] String to_string() const;
    static Result<Disposition> parse(StringView str);
};

// =============================================================================
// Space Allocation
// =============================================================================

enum class SpaceUnit : UInt8 {
    TRACKS = 1,
    CYLINDERS = 2,
    BLOCKS = 3,
    BYTES = 4,
    KILOBYTES = 5,
    MEGABYTES = 6,
    RECORDS = 7
};

struct SpaceAllocation {
    SpaceUnit unit = SpaceUnit::TRACKS;
    UInt32 primary = 0;
    UInt32 secondary = 0;
    UInt32 directory = 0;  // For PDS
    UInt32 block_size = 0; // For BLKSIZE
    bool rlse = false;     // Release unused space
    bool contig = false;   // Contiguous allocation
    bool round = false;    // Round to cylinder boundary
    
    [[nodiscard]] String to_string() const;
    static Result<SpaceAllocation> parse(StringView str);
};

// =============================================================================
// DCB (Data Control Block) Parameters
// =============================================================================

enum class RecordFormat : UInt8 {
    F = 1,      // Fixed
    FB = 2,     // Fixed Blocked
    V = 3,      // Variable
    VB = 4,     // Variable Blocked
    U = 5,      // Undefined
    FBA = 6,    // Fixed Blocked ASA
    VBA = 7,    // Variable Blocked ASA
    FM = 8,     // Fixed Machine
    VM = 9      // Variable Machine
};

enum class DatasetOrg : UInt8 {
    PS = 1,     // Physical Sequential
    PO = 2,     // Partitioned (PDS)
    DA = 3,     // Direct Access
    IS = 4,     // Indexed Sequential (ISAM)
    VS = 5      // VSAM
};

struct DCBParameters {
    Optional<RecordFormat> recfm;
    Optional<UInt32> lrecl;
    Optional<UInt32> blksize;
    Optional<DatasetOrg> dsorg;
    Optional<UInt32> bufno;
    String like_dsn;  // Copy DCB from existing dataset
    
    [[nodiscard]] String to_string() const;
    static Result<DCBParameters> parse(StringView str);
};

// =============================================================================
// DD Statement Parameters
// =============================================================================

struct DDParameters {
    // Dataset identification
    String dsn;                          // Dataset name
    String member;                       // PDS member
    bool temporary = false;              // &&name
    bool referback = false;              // *.stepname.ddname
    String referback_step;
    String referback_dd;
    
    // Disposition
    Optional<Disposition> disp;
    
    // Space allocation
    Optional<SpaceAllocation> space;
    
    // DCB parameters
    Optional<DCBParameters> dcb;
    
    // Unit/Volume
    String unit;
    String volume;
    bool volume_ref = false;
    String volume_ref_step;
    String volume_ref_dd;
    
    // SMS classes
    String storclas;
    String mgmtclas;
    String dataclas;
    
    // Other
    String sysout;                       // SYSOUT class
    String hold;                         // Hold output
    String dest;                         // Destination
    String copies;                       // Number of copies
    bool dummy = false;                  // DUMMY
    String path;                         // Unix path
    String pathdisp;                     // Unix path disposition
    String pathopts;                     // Unix path options
    String filedata;                     // FILEDATA
    String label;                        // Tape label
    
    // Instream data
    bool instream = false;
    String instream_data;
    String instream_delimiter;
    
    [[nodiscard]] String to_string() const;
    [[nodiscard]] bool is_concatenation() const { return dsn.empty() && !dummy && !instream; }
};

// =============================================================================
// EXEC Statement Parameters
// =============================================================================

struct ExecParameters {
    // Program or procedure
    String pgm;                          // Program name
    String proc;                         // Procedure name
    
    // Parameters
    String parm;                         // Program parameters
    std::map<String, String> proc_parms; // Procedure symbolic parameters
    
    // Step control
    String cond;                         // Condition codes
    String region;                       // Memory region
    String time;                         // Time limit
    String acct;                         // Accounting info
    String addrspc;                      // Address space type
    String dynamnbr;                     // Dynamic allocation
    bool rd_r = false;                   // Restart - Rerun
    bool rd_nc = false;                  // Restart - No checkpoint
    bool rd_nck = false;                 // Restart - No checkpoint, keep
    
    // Performance
    String perform;                      // Performance group
    String dprty;                        // Dispatching priority
    
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// JOB Statement Parameters
// =============================================================================

struct JobParameters {
    String job_name;
    String account;
    String programmer;
    
    // Job control
    String class_name;                   // CLASS
    String msgclass;                     // MSGCLASS
    String msglevel;                     // MSGLEVEL
    String notify;                       // NOTIFY user
    String region;                       // Region size
    String time;                         // Time limit
    String cond;                         // Condition code
    String typrun;                       // SCAN, HOLD, COPY, etc.
    String prty;                         // Priority
    
    // Restart/Recovery
    String restart;                      // Restart step
    String rd;                           // Restart definition
    
    // Security
    String user;                         // User ID
    String password;                     // Password (deprecated)
    String group;                        // Group name
    String seclabel;                     // Security label
    
    // Output
    String bytes;                        // Output limit in bytes
    String lines;                        // Output limit in lines
    String pages;                        // Output limit in pages
    String cards;                        // Output limit in cards
    
    // Scheduling
    String schenv;                       // Scheduling environment
    String system;                       // System affinity
    String jeslog;                       // JES log disposition
    
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// JCL Statement
// =============================================================================

struct JCLStatement {
    StatementType type = StatementType::UNKNOWN;
    String name;                         // Statement name (label)
    String operation;                    // Operation field
    String operands;                     // Operand field (raw)
    String comment;                      // Comment field
    UInt32 line_number = 0;
    bool continuation = false;
    
    // Parsed parameters based on type
    std::variant<
        std::monostate,
        JobParameters,
        ExecParameters,
        DDParameters
    > parameters;
    
    [[nodiscard]] String to_string() const;
    [[nodiscard]] bool is_valid() const { return type != StatementType::UNKNOWN; }
};

// =============================================================================
// JCL Step
// =============================================================================

struct JCLStep {
    String step_name;
    ExecParameters exec;
    std::vector<std::pair<String, DDParameters>> dd_statements;
    UInt32 step_number = 0;
    bool is_proc_step = false;
    String proc_name;
    
    [[nodiscard]] Optional<const DDParameters*> get_dd(StringView ddname) const;
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// JCL Job
// =============================================================================

struct JCLJob {
    JobParameters job_params;
    std::vector<JCLStep> steps;
    std::vector<JCLStatement> all_statements;
    std::map<String, String> symbols;    // Symbolic substitutions
    
    [[nodiscard]] Optional<const JCLStep*> get_step(StringView step_name) const;
    [[nodiscard]] Optional<const JCLStep*> get_step(UInt32 step_number) const;
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_json() const;
};

// =============================================================================
// Parser Errors
// =============================================================================

enum class JCLError : UInt16 {
    OK = 0,
    SYNTAX_ERROR = 1,
    INVALID_LABEL = 2,
    INVALID_OPERATION = 3,
    MISSING_OPERAND = 4,
    INVALID_KEYWORD = 5,
    UNBALANCED_PARENS = 6,
    UNBALANCED_QUOTES = 7,
    UNDEFINED_SYMBOL = 8,
    INVALID_DSN = 9,
    INVALID_DISPOSITION = 10,
    CONTINUATION_ERROR = 11,
    DUPLICATE_LABEL = 12,
    MISSING_JOB = 13,
    MISSING_EXEC = 14,
    PROC_NOT_FOUND = 15,
    INCLUDE_NOT_FOUND = 16,
    RECURSIVE_INCLUDE = 17,
    MAX_NESTING_EXCEEDED = 18
};

[[nodiscard]] constexpr StringView to_string(JCLError err) {
    switch (err) {
        case JCLError::OK: return "OK";
        case JCLError::SYNTAX_ERROR: return "SYNTAX_ERROR";
        case JCLError::INVALID_LABEL: return "INVALID_LABEL";
        case JCLError::INVALID_OPERATION: return "INVALID_OPERATION";
        case JCLError::MISSING_OPERAND: return "MISSING_OPERAND";
        case JCLError::INVALID_KEYWORD: return "INVALID_KEYWORD";
        case JCLError::UNBALANCED_PARENS: return "UNBALANCED_PARENS";
        case JCLError::UNBALANCED_QUOTES: return "UNBALANCED_QUOTES";
        case JCLError::UNDEFINED_SYMBOL: return "UNDEFINED_SYMBOL";
        case JCLError::INVALID_DSN: return "INVALID_DSN";
        case JCLError::INVALID_DISPOSITION: return "INVALID_DISPOSITION";
        case JCLError::CONTINUATION_ERROR: return "CONTINUATION_ERROR";
        case JCLError::DUPLICATE_LABEL: return "DUPLICATE_LABEL";
        case JCLError::MISSING_JOB: return "MISSING_JOB";
        case JCLError::MISSING_EXEC: return "MISSING_EXEC";
        case JCLError::PROC_NOT_FOUND: return "PROC_NOT_FOUND";
        case JCLError::INCLUDE_NOT_FOUND: return "INCLUDE_NOT_FOUND";
        case JCLError::RECURSIVE_INCLUDE: return "RECURSIVE_INCLUDE";
        case JCLError::MAX_NESTING_EXCEEDED: return "MAX_NESTING_EXCEEDED";
    }
    return "UNKNOWN";
}

struct ParseError {
    JCLError code = JCLError::OK;
    UInt32 line = 0;
    UInt32 column = 0;
    String message;
    String context;
    
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// JCL Parser Options
// =============================================================================

struct ParserOptions {
    bool strict_mode = false;            // Strict syntax checking
    bool expand_procs = true;            // Expand procedure calls
    bool resolve_symbols = true;         // Resolve symbolic parameters
    bool validate_dsnames = false;       // Validate dataset names
    Size max_include_depth = 10;         // Maximum INCLUDE nesting
    Size max_continuation_lines = 255;   // Maximum continuation lines
    Path proc_library;                   // PROCLIB path
    Path include_library;                // Include library path
    std::map<String, String> default_symbols; // Default symbolic values
};

// =============================================================================
// JCL Parser
// =============================================================================

class JCLParser {
private:
    ParserOptions options_;
    std::vector<ParseError> errors_;
    std::vector<ParseError> warnings_;
    std::map<String, String> symbols_;
    UInt32 current_line_ = 0;
    
    // Parsing helpers
    Result<JCLStatement> parse_statement(StringView line);
    Result<StatementType> identify_statement(StringView operation);
    Result<JobParameters> parse_job_params(StringView operands);
    Result<ExecParameters> parse_exec_params(StringView operands);
    Result<DDParameters> parse_dd_params(StringView operands);
    
    // Symbol handling
    String substitute_symbols(StringView str);
    void set_symbol(StringView name, StringView value);
    Optional<String> get_symbol(StringView name) const;
    
    // Utility
    std::pair<String, String> split_keyword_value(StringView param);
    std::vector<String> parse_keyword_list(StringView str);
    bool is_valid_label(StringView label) const;
    bool is_valid_dsname(StringView dsn) const;
    
    void add_error(JCLError code, StringView message, StringView context = "");
    void add_warning(JCLError code, StringView message, StringView context = "");
    
public:
    explicit JCLParser(ParserOptions options = {});
    
    // Parse JCL
    Result<JCLJob> parse(StringView jcl);
    Result<JCLJob> parse_file(const Path& path);
    
    // Parse individual statements
    Result<JCLStatement> parse_single_statement(StringView line);
    
    // Error access
    [[nodiscard]] const std::vector<ParseError>& errors() const { return errors_; }
    [[nodiscard]] const std::vector<ParseError>& warnings() const { return warnings_; }
    [[nodiscard]] bool has_errors() const { return !errors_.empty(); }
    [[nodiscard]] bool has_warnings() const { return !warnings_.empty(); }
    
    // Clear state
    void reset();
    
    // Options
    [[nodiscard]] const ParserOptions& options() const { return options_; }
    void set_options(ParserOptions options) { options_ = std::move(options); }
    
    // Symbol management
    void add_symbol(StringView name, StringView value);
    void clear_symbols();
};

// =============================================================================
// JCL Validator
// =============================================================================

class JCLValidator {
private:
    std::vector<ParseError> errors_;
    std::vector<ParseError> warnings_;
    
public:
    Result<void> validate(const JCLJob& job);
    
    // Individual validations
    Result<void> validate_job_params(const JobParameters& params);
    Result<void> validate_exec_params(const ExecParameters& params);
    Result<void> validate_dd_params(const DDParameters& params);
    Result<void> validate_step(const JCLStep& step);
    Result<void> validate_dsname(StringView dsn);
    
    [[nodiscard]] const std::vector<ParseError>& errors() const { return errors_; }
    [[nodiscard]] const std::vector<ParseError>& warnings() const { return warnings_; }
    
    void reset();
};

// =============================================================================
// JCL Generator (for creating JCL from objects)
// =============================================================================

class JCLGenerator {
private:
    String output_;
    UInt32 current_column_ = 0;
    
    void emit(StringView str);
    void emit_line(StringView str);
    void emit_continuation();
    void emit_keyword(StringView keyword, StringView value);
    
public:
    String generate(const JCLJob& job);
    String generate(const JCLStatement& stmt);
    String generate(const JobParameters& params, StringView job_name);
    String generate(const ExecParameters& params, StringView step_name);
    String generate(const DDParameters& params, StringView dd_name);
    
    void reset();
};

// =============================================================================
// Utility Functions
// =============================================================================

// Parse a DSN string into components
struct DSNComponents {
    String high_level_qualifier;
    std::vector<String> qualifiers;
    String member;
    bool is_temporary = false;
    bool is_gdg = false;
    Int32 gdg_generation = 0;  // +1, 0, -1, etc.
    
    [[nodiscard]] String full_dsn() const;
};

[[nodiscard]] Result<DSNComponents> parse_dsn(StringView dsn);
[[nodiscard]] bool is_valid_dsn(StringView dsn);
[[nodiscard]] bool is_valid_member_name(StringView member);
[[nodiscard]] String normalize_dsn(StringView dsn);

} // namespace cics::jcl
