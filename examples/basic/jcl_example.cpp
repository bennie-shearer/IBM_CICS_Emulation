// =============================================================================
// CICS Emulation - JCL Parser Example
// Version: 3.4.6
// =============================================================================
// Demonstrates JCL parsing, validation, and generation
// =============================================================================

#include <iostream>
#include <iomanip>
#include <cics/jcl/jcl_parser.hpp>

using namespace cics;
using namespace cics::jcl;

void print_separator(const char* title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << " " << title << "\n";
    std::cout << std::string(70, '=') << "\n";
}

void print_job_summary(const JCLJob& job) {
    std::cout << "  Job Name: " << job.job_params.job_name << "\n";
    std::cout << "  Account:  " << (job.job_params.account.empty() ? "(none)" : job.job_params.account) << "\n";
    std::cout << "  Class:    " << job.job_params.class_name << "\n";
    std::cout << "  Steps:    " << job.steps.size() << "\n";
    
    for (size_t i = 0; i < job.steps.size(); ++i) {
        const auto& step = job.steps[i];
        std::cout << "\n  Step " << (i + 1) << ": " << step.step_name << "\n";
        if (!step.exec.pgm.empty()) {
            std::cout << "    PGM=" << step.exec.pgm << "\n";
        }
        if (!step.exec.proc.empty()) {
            std::cout << "    PROC=" << step.exec.proc << "\n";
        }
        std::cout << "    DD Statements: " << step.dd_statements.size() << "\n";
        
        for (const auto& [dd_name, params] : step.dd_statements) {
            std::cout << "      " << std::setw(8) << std::left << dd_name;
            if (!params.dsn.empty()) {
                std::cout << " DSN=" << params.dsn;
            }
            if (!params.sysout.empty()) {
                std::cout << " SYSOUT=" << params.sysout;
            }
            std::cout << "\n";
        }
    }
}

int main() {
    std::cout << R"(
+======================================================================+
|         CICS Emulation - JCL Parser Example               |
|                          Version 3.4.6                               |
+======================================================================+
)";

    print_separator("1. Parsing a Simple JCL Job");
    
    // Sample JCL for a simple batch job
    String simple_jcl = R"(//MYJOB    JOB (ACCT123),'BATCH JOB',CLASS=A,MSGCLASS=X
//STEP1    EXEC PGM=IEFBR14
//SYSPRINT DD SYSOUT=*
//SYSUDUMP DD SYSOUT=*
)";
    
    std::cout << "  Input JCL:\n";
    std::cout << "  " << std::string(50, '-') << "\n";
    std::istringstream iss(simple_jcl);
    String line;
    while (std::getline(iss, line)) {
        std::cout << "  " << line << "\n";
    }
    std::cout << "  " << std::string(50, '-') << "\n\n";
    
    JCLParser parser;
    auto result = parser.parse(simple_jcl);
    
    if (result) {
        std::cout << "  Parse successful!\n\n";
        print_job_summary(result.value());
    } else {
        std::cerr << "  Parse failed: " << result.error().message << "\n";
    }

    print_separator("2. Parsing JCL with Dataset Allocation");
    
    String dataset_jcl = R"(//ALLOCJOB JOB ,'DATASET ALLOC',CLASS=A
//STEP1    EXEC PGM=IEBGENER
//SYSUT1   DD DSN=INPUT.DATA.SET,DISP=SHR
//SYSUT2   DD DSN=OUTPUT.DATA.SET,
//            DISP=(NEW,CATLG,DELETE),
//            SPACE=(TRK,(10,5),RLSE),
//            DCB=(RECFM=FB,LRECL=80,BLKSIZE=8000)
//SYSPRINT DD SYSOUT=*
//SYSIN    DD DUMMY
)";
    
    std::cout << "  Parsing JCL with dataset allocation parameters...\n\n";
    
    auto result2 = parser.parse(dataset_jcl);
    if (result2) {
        print_job_summary(result2.value());
        
        // Show parsed disposition details
        if (!result2.value().steps.empty()) {
            for (const auto& [dd_name, params] : result2.value().steps[0].dd_statements) {
                if (dd_name == "SYSUT2" && params.disp.has_value()) {
                    std::cout << "\n  SYSUT2 Disposition Details:\n";
                    std::cout << "    Status: ";
                    switch (params.disp->status) {
                        case DatasetStatus::NEW: std::cout << "NEW"; break;
                        case DatasetStatus::OLD: std::cout << "OLD"; break;
                        case DatasetStatus::SHR: std::cout << "SHR"; break;
                        case DatasetStatus::MOD: std::cout << "MOD"; break;
                    }
                    std::cout << "\n";
                }
            }
        }
    }

    print_separator("3. Parsing Disposition Strings");
    
    // Demonstrate disposition parsing
    std::vector<String> disp_examples = {
        "(NEW,CATLG,DELETE)",
        "(OLD,KEEP)",
        "(SHR)",
        "(MOD,CATLG,CATLG)"
    };
    
    for (const auto& disp_str : disp_examples) {
        auto disp_result = Disposition::parse(disp_str);
        if (disp_result) {
            const auto& disp = disp_result.value();
            std::cout << "  " << std::setw(22) << std::left << disp_str << " -> ";
            
            std::cout << "Status=";
            switch (disp.status) {
                case DatasetStatus::NEW: std::cout << "NEW"; break;
                case DatasetStatus::OLD: std::cout << "OLD"; break;
                case DatasetStatus::SHR: std::cout << "SHR"; break;
                case DatasetStatus::MOD: std::cout << "MOD"; break;
            }
            
            std::cout << ", Normal=";
            switch (disp.normal) {
                case NormalDisposition::DELETE: std::cout << "DELETE"; break;
                case NormalDisposition::KEEP: std::cout << "KEEP"; break;
                case NormalDisposition::PASS: std::cout << "PASS"; break;
                case NormalDisposition::CATLG: std::cout << "CATLG"; break;
                case NormalDisposition::UNCATLG: std::cout << "UNCATLG"; break;
            }
            
            std::cout << "\n";
        }
    }

    print_separator("4. Symbol Substitution");
    
    // JCL with symbolic parameters - demonstrate the concept
    String symbolic_jcl = R"(//SYMBJOB  JOB ,'SYMBOLIC TEST'
// SET INDSN='MY.INPUT.DATA'
// SET OUTDSN='MY.OUTPUT.DATA'
//STEP1    EXEC PGM=IEBCOPY
//SYSUT1   DD DSN=&INDSN,DISP=SHR
//SYSUT2   DD DSN=&OUTDSN,DISP=(NEW,CATLG)
//SYSPRINT DD SYSOUT=*
)";
    
    std::cout << "  JCL with symbolic parameters (// SET statements):\n";
    std::cout << "    &INDSN  - defined via SET statement\n";
    std::cout << "    &OUTDSN - defined via SET statement\n\n";
    
    // Parser handles SET statements internally during parse
    auto sym_result = parser.parse(symbolic_jcl);
    if (sym_result) {
        std::cout << "  Parsed job with symbol substitution:\n";
        print_job_summary(sym_result.value());
    }

    print_separator("5. JCL Validation");
    
    // Validate a parsed job
    JCLValidator validator;
    
    if (result) {
        auto valid_result = validator.validate(result.value());
        if (valid_result) {
            std::cout << "  Validation passed for MYJOB\n";
        } else {
            std::cout << "  Validation issues found:\n";
            for (const auto& err : validator.errors()) {
                std::cout << "    Line " << err.line << ": " << err.message << "\n";
            }
        }
    }

    print_separator("6. JCL Generation");
    
    // Build a job programmatically and generate JCL
    JCLJob new_job;
    new_job.job_params.job_name = "GENJOB";
    new_job.job_params.account = "DEVACCT";
    new_job.job_params.programmer = "CLAUDE";
    new_job.job_params.class_name = "A";
    new_job.job_params.msgclass = "X";
    
    // Add a step
    JCLStep step1;
    step1.step_name = "COMPILE";
    step1.exec.pgm = "IGYCRCTL";  // COBOL compiler
    
    // Add DD statements as pairs of (name, params)
    DDParameters sysin_params;
    sysin_params.dsn = "SOURCE.COBOL(PROGRAM1)";
    Disposition sysin_disp;
    sysin_disp.status = DatasetStatus::SHR;
    sysin_params.disp = sysin_disp;
    step1.dd_statements.emplace_back("SYSIN", sysin_params);
    
    DDParameters syslib_params;
    syslib_params.dsn = "COPY.LIBRARY";
    Disposition syslib_disp;
    syslib_disp.status = DatasetStatus::SHR;
    syslib_params.disp = syslib_disp;
    step1.dd_statements.emplace_back("SYSLIB", syslib_params);
    
    DDParameters sysprint_params;
    sysprint_params.sysout = "*";
    step1.dd_statements.emplace_back("SYSPRINT", sysprint_params);
    
    new_job.steps.push_back(step1);
    
    // Generate JCL
    JCLGenerator generator;
    String generated_jcl = generator.generate(new_job);
    
    std::cout << "  Generated JCL:\n";
    std::cout << "  " << std::string(50, '-') << "\n";
    std::istringstream gen_iss(generated_jcl);
    while (std::getline(gen_iss, line)) {
        std::cout << "  " << line << "\n";
    }
    std::cout << "  " << std::string(50, '-') << "\n";

    print_separator("7. DSN Parsing");
    
    // Parse dataset names - show component parts
    std::vector<String> dsn_examples = {
        "SYS1.LINKLIB",
        "USER.DATA.SET",
        "PROD.LIBRARY(MEMBER)",
        "MY.GDG.BASE(+1)",
        "&&TEMPDS"
    };
    
    std::cout << "  Sample dataset names:\n";
    for (const auto& dsn : dsn_examples) {
        std::cout << "    " << dsn << "\n";
    }
    std::cout << "\n  (DSN parsing extracts HLQ, qualifiers, members, GDG info)\n";

    print_separator("8. Error Handling");
    
    // Show error handling
    if (parser.has_errors()) {
        std::cout << "  Parser has errors\n";
    } else {
        std::cout << "  No parsing errors\n";
    }
    
    if (parser.has_warnings()) {
        std::cout << "  Parser has warnings\n";
    } else {
        std::cout << "  No parsing warnings\n";
    }

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << " JCL Parser Example completed successfully!\n";
    std::cout << std::string(70, '=') << "\n\n";

    return 0;
}
