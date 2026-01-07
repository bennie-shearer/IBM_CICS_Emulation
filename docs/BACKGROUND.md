# IBM CICS (Customer Information Control System) Emulation - Background
Version 3.4.6

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [The History and Purpose of CICS by IBM](#2-the-history-and-purpose-of-cics-by-ibm)
   - 2.1 [Origins at IBM (1968-1970s)](#21-origins-at-ibm-1968-1970s)
   - 2.2 [Evolution Through the Decades](#22-evolution-through-the-decades)
   - 2.3 [Core CICS Architecture](#23-core-cics-architecture)
   - 2.4 [CICS in Modern Mainframe Environments](#24-cics-in-modern-mainframe-environments)
   - 2.5 [The EXEC CICS API](#25-the-exec-cics-api)
3. [Why Emulation is Valuable](#3-why-emulation-is-valuable)
   - 3.1 [Modernization](#31-modernization)
   - 3.2 [Training](#32-training)
   - 3.3 [Development](#33-development)
   - 3.4 [Testing](#34-testing)
   - 3.5 [Cost Benefits](#35-cost-benefits)
4. [How This Project Fits Into the Broader Mainframe Ecosystem](#4-how-this-project-fits-into-the-broader-mainframe-ecosystem)
   - 4.1 [The Mainframe Technology Stack](#41-the-mainframe-technology-stack)
   - 4.2 [Related Technologies We Emulate](#42-related-technologies-we-emulate)
   - 4.3 [Integration Points](#43-integration-points)
   - 4.4 [Complementary Tools](#44-complementary-tools)
5. [Design Philosophy](#5-design-philosophy)
   - 5.1 [Modern C++ (C++20)](#51-modern-c-c20)
   - 5.2 [Zero External Dependencies](#52-zero-external-dependencies)
   - 5.3 [Cross-Platform Compatibility](#53-cross-platform-compatibility)
   - 5.4 [API Fidelity](#54-api-fidelity)
   - 5.5 [Performance and Efficiency](#55-performance-and-efficiency)
   - 5.6 [Extensibility and Modularity](#56-extensibility-and-modularity)
6. [Module Overview](#6-module-overview)
7. [Use Cases](#7-use-cases)
8. [Conclusion](#8-conclusion)
9. [References](#9-references)
10. [License](#10-license)
11. [Author](#11-author)
12. [Acknowledgments](#12-acknowledgments)

---

## 1. Introduction

CICS Emulation Enterprise is a comprehensive, production-quality emulation of IBM's Customer Information Control System (CICS) transaction processing environment. This project provides a faithful implementation of core CICS services in modern C++20, enabling developers, educators, and organizations to work with CICS concepts and APIs without requiring access to actual mainframe hardware.

The emulation covers 27 CICS modules including VSAM file handling, temporary and transient data queues, program control, interval control, task control, storage management, syncpoint processing, abend handling, channel/container communication, terminal control, BMS mapping, spool control, document handling, inquiry services, named counters, and journal control.

This document provides essential background on CICS, explains the value of emulation, describes how this project fits into the mainframe ecosystem, and outlines the design principles that guide its development.

---

## 2. The History and Purpose of CICS by IBM

### 2.1 Origins at IBM (1968-1970s)

CICS (Customer Information Control System) was born out of necessity. In the late 1960s, IBM customers needed a way to run interactive applications on their mainframe computers, which were originally designed for batch processing. Before CICS, mainframe applications processed jobs in queues and users submitted work and waited hours or days for results.

**Key Historical Milestones:**

| Year | Event |
|------|-------|
| 1968 | CICS developed by IBM's Public Utilities division in Des Plaines, Illinois |
| 1969 | First release as a licensed program for OS/360 |
| 1970 | Rapid adoption by financial institutions for online banking |
| 1972 | CICS/VS introduced with virtual storage support |
| 1974 | Transaction routing capabilities added |
| 1977 | CICS/VS Version 1.5 with improved performance |

The original CICS solved a fundamental problem: how to allow multiple users to interact with a mainframe simultaneously, each running their own transaction, while sharing system resources efficiently. This was revolutionary because it transformed mainframes from batch processing engines into interactive systems capable of supporting thousands of concurrent users.

### 2.2 Evolution Through the Decades

**1980s: Enterprise Dominance**

CICS became the de facto standard for online transaction processing (OLTP) in large enterprises. Banks, airlines, retailers, and government agencies built their core business systems on CICS. Key developments included:

- CICS/ESA with extended addressing capabilities
- Improved resource management and memory handling
- Enhanced security features with RACF integration
- Distributed transaction processing (DTP) for multi-region communication
- Improved recovery and restart capabilities

**1990s: Client/Server Integration**

As client/server computing emerged, CICS adapted to remain relevant:

- CICS Client products for PC connectivity
- External Call Interface (EXCI) for batch integration
- CICS Gateway for Java enabling web connectivity
- Web enablement through CICS Web Interface
- Object-oriented programming support

**2000s: Web Services and Modern Integration**

CICS transformed to support modern service-oriented architectures:

- SOAP and REST web services support
- JSON data format handling
- Atom feeds for event distribution
- Enhanced Java support via Liberty Profile
- Event processing capabilities for real-time analytics

**2010s-Present: Cloud and API Economy**

Current CICS versions embrace cloud-native patterns while maintaining backward compatibility:

- Container deployment options
- RESTful API management and exposure
- DevOps integration with CI/CD pipelines
- Hybrid cloud connectivity
- OpenAPI specification support
- Improved scalability and performance

### 2.3 Core CICS Architecture

Understanding CICS requires familiarity with its fundamental architectural concepts:

**Region Architecture**

A CICS region is an address space that provides the runtime environment for transactions. Multiple regions can cooperate:

| Region Type | Purpose |
|-------------|---------|
| AOR (Application Owning Region) | Runs application programs |
| TOR (Terminal Owning Region) | Manages terminal connections |
| FOR (File Owning Region) | Owns and manages files |
| DOR (Data Owning Region) | Manages data resources |

**Transaction Processing**

A CICS transaction is a unit of work triggered by user input or program request. Transactions are:
- Short-lived (typically milliseconds to seconds)
- Atomic (all-or-nothing completion)
- Recoverable (can be rolled back on failure)
- Identified by a 4-character transaction ID

**Resource Management**

CICS manages various resources with different lifecycle characteristics:

| Resource | Description | Lifecycle |
|----------|-------------|-----------|
| Programs | COBOL, PL/I, Assembler, C/C++, Java | Loaded on demand, cached |
| Files | VSAM datasets (KSDS, ESDS, RRDS) | Opened per region |
| TSQs | Temporary Storage Queues | Task or region lifetime |
| TDQs | Transient Data Queues | Persistent or temporary |
| Terminals | 3270 displays and printers | Session-based |
| Connections | Inter-region and external systems | Configured |

### 2.4 CICS in Modern Mainframe Environments

Despite being over 55 years old, CICS remains vital to the global economy:

**Scale and Impact:**

- Processes over 1.2 million transactions per second globally
- Handles an estimated $1 trillion in daily financial transactions
- Powers approximately 30 billion transactions daily
- Used by 92 of the world's top 100 banks
- Runs at 9 of the top 10 global retailers
- Supports critical government and healthcare systems

**Why CICS Endures:**

1. **Reliability**: Five-nines availability (99.999% uptime)
2. **Performance**: Sub-millisecond response times for complex transactions
3. **Scalability**: Handles millions of concurrent users
4. **Security**: Enterprise-grade access control and audit trails
5. **Transactional Integrity**: Full ACID compliance guaranteed
6. **Investment Protection**: Decades of business logic preserved

### 2.5 The EXEC CICS API

Programs interact with CICS through the EXEC CICS API, which is what our emulation faithfully reproduces:

```cobol
EXEC CICS READ FILE('CUSTFILE')
           INTO(CUSTOMER-RECORD)
           RIDFLD(CUSTOMER-KEY)
           RESP(WS-RESP)
END-EXEC
```

The API covers hundreds of commands across categories:

| Category | Commands |
|----------|----------|
| File Control | READ, WRITE, REWRITE, DELETE, BROWSE |
| Program Control | LINK, XCTL, RETURN, LOAD |
| Interval Control | ASKTIME, DELAY, POST, WAIT |
| Queue Operations | WRITEQ TS, READQ TS, DELETEQ TS |
| Storage Control | GETMAIN, FREEMAIN |
| Terminal Control | SEND, RECEIVE, CONVERSE |

---

## 3. Why Emulation is Valuable

### 3.1 Modernization

Mainframe modernization is a strategic imperative for many organizations. CICS emulation enables several approaches:

**Strangler Fig Pattern**

Gradually replace CICS functionality with modern services:
1. Use emulation to understand existing behavior
2. Build new services that replicate functionality
3. Route traffic progressively to new services
4. Eventually retire legacy components

**Lift and Shift**

Migrate CICS applications to distributed platforms:
1. Emulation provides the runtime environment
2. Recompile or convert COBOL to another language
3. Validate behavior against emulation
4. Deploy on cloud infrastructure

**Hybrid Coexistence**

Operate emulated and real CICS side-by-side:
1. Development and testing on emulation
2. Production on real mainframe
3. Gradual migration as confidence grows

**API Extraction**

Expose legacy logic as modern APIs:
1. Understand transaction flows via emulation
2. Design RESTful wrappers
3. Test integration patterns
4. Deploy API gateways

### 3.2 Training

**The Skills Gap Problem**

The average mainframe programmer is over 50 years old. As experienced developers retire, organizations face a critical skills shortage. Emulation addresses this by:

- Providing accessible learning environments on standard hardware
- Eliminating the need for expensive mainframe access
- Enabling hands-on practice with CICS concepts
- Supporting academic curriculum development
- Allowing self-paced learning

**Training Benefits Comparison**

| Traditional Approach | With Emulation |
|---------------------|----------------|
| Requires mainframe access (expensive) | Runs on laptops and workstations |
| Limited practice time due to cost | Unlimited experimentation |
| Scheduling conflicts for lab time | Available 24/7 |
| Risk of impacting production | Completely isolated |
| Complex setup and configuration | Simple build and run |
| Geographic constraints | Learn anywhere |

### 3.3 Development

Emulation accelerates development cycles in multiple ways:

**Rapid Prototyping**

- Experiment with CICS patterns without mainframe overhead
- Test architectural decisions before committing resources
- Validate migration strategies quickly
- Build proof-of-concept implementations

**Local Development**

- Develop and debug on personal workstations
- Use familiar IDEs and debugging tools
- Iterate quickly without mainframe connectivity
- Test changes immediately

**Continuous Integration**

- Include CICS logic in CI/CD pipelines
- Automate testing of transaction logic
- Validate changes before mainframe deployment
- Reduce integration cycle time

### 3.4 Testing

**Unit Testing**

- Test individual CICS programs in isolation
- Mock external dependencies
- Verify correct API usage
- Measure code coverage

**Integration Testing**

- Test multi-program transactions
- Validate inter-component communication
- Verify data flow correctness
- Test error handling paths

**Regression Testing**

- Ensure changes do not break existing functionality
- Compare behavior between versions
- Automate test execution
- Generate test reports

**Performance Baseline**

- Establish baseline performance metrics
- Identify bottlenecks in logic
- Optimize before mainframe deployment
- Compare optimization approaches

### 3.5 Cost Benefits

| Cost Category | Mainframe Approach | Emulation Approach |
|--------------|-------------------|-------------------|
| Hardware | Millions in hardware costs | Standard PCs |
| MIPS/MSU Charges | Pay-per-use licensing | No usage charges |
| Development Licenses | Per-seat licensing | MIT license (free) |
| Training Time | On-mainframe practice costs | Unlimited practice |
| Iteration Speed | Hours/days for changes | Minutes for changes |

---

## 4. How This Project Fits Into the Broader Mainframe Ecosystem

### 4.1 The Mainframe Technology Stack

The mainframe ecosystem is a comprehensive stack of integrated technologies:

```
+------------------------------------------------------------------+
|                    User Interface Layer                          |
|        3270 Terminals | Web Browsers | Mobile Apps              |
+------------------------------------------------------------------+
|                    Transaction Layer                             |
|                    CICS | IMS/TM | Batch                         |
+------------------------------------------------------------------+
|                    Data Access Layer                             |
|        VSAM | DB2 | IMS/DB | Sequential Files | GDG            |
+------------------------------------------------------------------+
|                    System Services Layer                         |
|        JES2/JES3 | SMS | RACF | HSM | Catalog                   |
+------------------------------------------------------------------+
|                    Operating System Layer                        |
|                         z/OS                                     |
+------------------------------------------------------------------+
|                    Hardware Layer                                |
|               IBM Z Series Mainframes                            |
+------------------------------------------------------------------+
```

This project emulates components primarily in the Transaction Layer and Data Access Layer.

### 4.2 Related Technologies We Emulate

| Technology | Our Module | Coverage |
|------------|------------|----------|
| CICS Transaction Processing | cics-core | Core EIB, transactions, programs |
| VSAM File Management | vsam | KSDS, ESDS, RRDS, LDS operations |
| Temporary Storage | tsq | WRITEQ TS, READQ TS, DELETEQ TS |
| Transient Data | tdq | Intrapartition and extrapartition |
| DFSMShsm | dfsmshsm | Migration, recall, backup concepts |
| Master Catalog | master-catalog | Dataset registration, lookup |
| GDG | gdg | Generation data group management |
| JCL | jcl | Job control language parsing |

### 4.3 Integration Points

The emulation provides integration with:

**Modern Development Tools**
- CLion, Visual Studio, VS Code
- CMake build system
- Standard debuggers (GDB, LLDB, Visual Studio Debugger)
- Static analyzers and linters

**Testing Frameworks**
- Built-in test framework (no external dependencies)
- Benchmark framework for performance testing
- Integration test support

**Data Formats**
- EBCDIC/ASCII conversion
- Packed decimal handling
- Fixed-length record processing
- COBOL copybook parsing

### 4.4 Complementary Tools

This project works alongside:

| Tool Category | Purpose |
|--------------|---------|
| COBOL Compilers | GnuCOBOL for open-source COBOL compilation |
| Data Conversion | Tools for mainframe data migration |
| Modernization Platforms | Commercial mainframe modernization suites |
| API Gateways | Expose emulated services as APIs |

---

## 5. Design Philosophy

### 5.1 Modern C++ (C++20)

The implementation leverages C++20 features for safety, performance, and expressiveness:

**Smart Pointers and RAII**

```cpp
class ResourceManager {
    std::unique_ptr<Resource> resource_;
public:
    ResourceManager() : resource_(std::make_unique<Resource>()) {}
    // Automatic cleanup when manager goes out of scope
};
```

**std::optional for Nullable Values**

```cpp
std::optional<CustomerRecord> find_customer(const std::string& id) {
    if (auto it = customers_.find(id); it != customers_.end()) {
        return it->second;
    }
    return std::nullopt;
}
```

**std::variant for Type-Safe Unions**

```cpp
using FieldValue = std::variant<int64_t, double, std::string, ByteBuffer>;
```

**Concepts and Constraints**

```cpp
template<typename T>
concept Serializable = requires(T t, ByteBuffer& buf) {
    { t.serialize(buf) } -> std::same_as<void>;
    { T::deserialize(buf) } -> std::same_as<T>;
};
```

**std::span for Safe Array Views**

```cpp
void process_buffer(std::span<const uint8_t> data) {
    for (auto byte : data) {
        // Safe iteration without pointer arithmetic
    }
}
```

### 5.2 Zero External Dependencies

The project has no external dependencies beyond the C++20 standard library. This design choice offers significant benefits:

**Simplified Deployment**

- No dependency management complexity
- No version conflicts or diamond dependencies
- No security vulnerabilities from third-party code
- Single, self-contained build process

**Portability**

- Builds anywhere with a C++20 compliant compiler
- No platform-specific library requirements
- Consistent behavior across systems

**Licensing Clarity**

- No license compatibility concerns
- Clear intellectual property boundaries
- Simplified distribution and compliance

**Implementation Approach**

Where libraries might typically be used, we provide built-in alternatives:

| Typical Dependency | Our Approach |
|-------------------|--------------|
| JSON library | Built-in document handling |
| Testing framework | Custom minimal test framework |
| Logging library | Built-in configurable logging |
| Threading library | C++20 standard threading |
| Crypto library | Optional OpenSSL integration |
| UUID generation | Built-in UUID implementation |

### 5.3 Cross-Platform Compatibility

The emulation runs on Windows, Linux, and macOS with equal functionality:

**Platform Abstraction**

```cpp
#if defined(CICS_PLATFORM_WINDOWS)
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(CICS_PLATFORM_LINUX)
    #include <pthread.h>
    #include <unistd.h>
#elif defined(CICS_PLATFORM_MACOS)
    #include <pthread.h>
    #include <mach/mach_time.h>
#endif
```

**Tested Configurations**

| Platform | Compiler | Minimum Version |
|----------|----------|-----------------|
| Windows | MSVC | 2022 (19.29+) |
| Windows | MinGW-w64 | GCC 12+ |
| Linux | GCC | 12+ |
| Linux | Clang | 14+ |
| macOS | Apple Clang | 14+ |

**Build System**

CMake provides cross-platform build configuration with platform-specific optimizations:

```cmake
if(WIN32)
    target_compile_definitions(cics-common PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(APPLE)
    target_compile_options(cics-common PRIVATE -stdlib=libc++)
elseif(UNIX)
    target_link_libraries(cics-common PRIVATE pthread)
endif()
```

### 5.4 API Fidelity

We strive to match CICS API semantics as closely as possible:

**Naming Conventions**

CICS command names are preserved in function form:
- `exec_cics_read()` mirrors EXEC CICS READ
- `exec_cics_writeq_ts()` mirrors EXEC CICS WRITEQ TS
- `exec_cics_link()` mirrors EXEC CICS LINK

**Error Handling**

CICS conditions are represented as typed error codes:

```cpp
enum class ErrorCode {
    NORMAL,      // Normal completion
    NOTFND,      // Record not found (CICS NOTFND)
    DUPKEY,      // Duplicate key (CICS DUPKEY)
    LENGERR,     // Length error (CICS LENGERR)
    INVREQ,      // Invalid request (CICS INVREQ)
    ENDFILE,     // End of file (CICS ENDFILE)
    IOERR,       // I/O error (CICS IOERR)
    // ... etc.
};
```

**Data Types**

We provide CICS-style fixed-length strings and packed decimals:

```cpp
FixedString<8> transaction_id;   // COBOL PIC X(8)
PackedDecimal amount(9, 2);      // COBOL PIC S9(9)V99 COMP-3
```

### 5.5 Performance and Efficiency

While not matching mainframe performance, we optimize where it matters:

**Memory Efficiency**

- Minimal allocations in hot paths
- Object pooling for frequently-created items
- Efficient string handling with std::string_view
- Cache-friendly data structures

**Concurrency**

- Thread-safe data structures where needed
- Lock-free algorithms where possible
- Efficient mutex usage patterns with std::shared_mutex
- Thread pool for background operations

**I/O Optimization**

- Buffered file operations
- Batch writes where appropriate
- Efficient buffer management
- Lazy initialization patterns

### 5.6 Extensibility and Modularity

The architecture supports extension and customization:

**Module System**

Each CICS function area is a separate, independently buildable module:

```
libs/
  common/           # Shared utilities, types, error handling
  vsam/             # VSAM file emulation
  tsq/              # Temporary Storage Queue
  tdq/              # Transient Data Queue
  program-control/  # LINK, XCTL, RETURN
  interval-control/ # ASKTIME, DELAY, POST, WAIT
  task-control/     # ENQ, DEQ resource locking
  storage-control/  # GETMAIN, FREEMAIN
  syncpoint/        # SYNCPOINT, ROLLBACK
  abend-handler/    # HANDLE ABEND
  channel/          # Channel and Container support
  terminal-control/ # SEND, RECEIVE
  bms/              # Basic Mapping Support
  spool/            # JES spool handling
  document/         # DOCUMENT CREATE/INSERT/RETRIEVE
  inquire/          # INQUIRE services
  named-counter/    # Named counter sequences
  journal/          # Journal control
  ... and more
```

**Extension Points**

- Custom file handlers for alternative storage backends
- User exits for transaction monitoring
- Custom security providers
- Pluggable logging backends

---

## 6. Module Overview

| Module | Description | Key APIs |
|--------|-------------|----------|
| **cics-core** | Core CICS types, EIB, transaction management | Transaction, EIB, COMMAREA |
| **vsam** | VSAM file emulation | READ, WRITE, REWRITE, DELETE, BROWSE |
| **tsq** | Temporary Storage Queues | WRITEQ TS, READQ TS, DELETEQ TS |
| **tdq** | Transient Data Queues | WRITEQ TD, READQ TD |
| **program-control** | Program invocation | LINK, XCTL, RETURN, LOAD |
| **interval-control** | Time and scheduling | ASKTIME, DELAY, POST, WAIT, START |
| **task-control** | Resource locking | ENQ, DEQ |
| **storage-control** | Dynamic storage | GETMAIN, FREEMAIN |
| **syncpoint** | Transaction boundaries | SYNCPOINT, ROLLBACK |
| **abend-handler** | Error handling | HANDLE ABEND, PUSH, POP |
| **channel** | Modern data passing | PUT CONTAINER, GET CONTAINER |
| **terminal-control** | Terminal I/O | SEND, RECEIVE, CONVERSE |
| **bms** | Screen mapping | SEND MAP, RECEIVE MAP |
| **spool** | JES spool access | SPOOLOPEN, SPOOLREAD, SPOOLWRITE |
| **document** | Document handling | CREATE, INSERT, RETRIEVE |
| **inquire** | Resource queries | INQUIRE PROGRAM, FILE, TRANSACTION |
| **named-counter** | Sequence generation | GET, PUT, UPDATE COUNTER |
| **journal** | Audit logging | WRITE JOURNALNAME |
| **ebcdic** | Character conversion | ASCII/EBCDIC, packed decimal |
| **copybook** | Struct generation | COBOL copybook parsing |
| **dump** | Debugging | Hex dumps, comparisons |
| **jcl** | Job control | JCL parsing |
| **gdg** | Generation groups | GDG base and generation management |
| **dfsmshsm** | Storage management | Migration, recall concepts |
| **master-catalog** | Dataset catalog | Registration, lookup |
| **security** | Access control | Authentication, encryption |

---

## 7. Use Cases

**Enterprise Modernization Team**

A large bank is migrating CICS applications to cloud-native services. The team uses the emulation to:
- Understand existing transaction flows
- Develop microservices that replicate CICS behavior
- Test integration between old and new systems
- Train developers unfamiliar with mainframe concepts

**University Computer Science Department**

A university offers a course on enterprise computing. The emulation enables:
- Students to experience CICS concepts on their laptops
- Lab exercises without mainframe costs
- Practical assignments with realistic APIs
- Research into transaction processing

**Independent Software Vendor**

A company develops tools for mainframe modernization. The emulation supports:
- Product development and testing
- Customer demonstrations without mainframe access
- Integration validation
- Documentation and training materials

**Government Agency Migration Project**

A government agency is modernizing legacy systems. The emulation aids:
- Analysis of existing CICS applications
- Prototype development
- Testing migration approaches
- Staff training and knowledge transfer

**Consulting Firm Training Program**

A consulting firm needs to train new hires on mainframe technologies:
- Self-paced learning modules
- Hands-on exercises
- Certification preparation
- Client engagement preparation

---

## 8. Conclusion

CICS Emulation Enterprise fills an important gap in the mainframe modernization toolkit. By providing a faithful, accessible, and free implementation of core CICS services, it enables:

- **Developers** to work with CICS concepts locally
- **Organizations** to reduce mainframe development costs
- **Educators** to teach mainframe skills effectively
- **Architects** to prototype modernization approaches
- **Teams** to accelerate testing and quality assurance

The design philosophy of modern C++, zero external dependencies, and cross-platform support ensures the emulation is practical, maintainable, and widely accessible. While it does not replace production CICS systems, it provides immense value for development, testing, training, and modernization activities.

As mainframe systems continue to underpin critical infrastructure worldwide, tools like CICS Emulation Enterprise play an essential role in preserving knowledge, enabling innovation, and bridging the gap between legacy systems and modern development practices.

---

## 9. References

**IBM Documentation**

- IBM CICS Transaction Server Documentation
- IBM CICS Application Programming Guide
- IBM CICS System Definition Guide
- IBM CICS Recovery and Restart Guide
- IBM VSAM Administration Guide

**Industry Resources**

- Gartner: Mainframe Modernization Strategies
- Forrester: The Future of Mainframe Computing
- IDC: Mainframe Market Analysis

**Technical Standards**

- COBOL ISO/IEC 1989:2014
- SQL ISO/IEC 9075
- X/Open Distributed Transaction Processing

**Historical Context**

- "IBM's Early Computers" - MIT Press
- "CICS: A How-To for COBOL Programmers" - McGraw-Hill
- "The Mythical Man-Month" - Frederick Brooks

---

## 10. License

This project is licensed under the MIT License - see the LICENSE file for details.

------------------------------------------------------------------------------

Copyright (c) 2025 Bennie Shearer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

------------------------------------------------------------------------------

DISCLAIMER

This software provides mainframe emulation capabilities. Users are responsible
for proper configuration, security testing, and compliance with applicable
regulations and licensing requirements.

------------------------------------------------------------------------------

## 11. Author

Bennie Shearer (retired)

## 12. Acknowledgments

Thanks to all my C++ mentors through the years

Special thanks to:

| Organization | Website |
|--------------|---------|
| CICS by IBM | https://www.ibm.com/ |
| CLion by JetBrains s.r.o. | https://www.jetbrains.com/clion/ |
| Claude by Anthropic PBC | https://www.anthropic.com/ |
