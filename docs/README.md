# IBM CICS (Customer Information Control System) Emulation
Version 3.4.6
## Overview
### A comprehensive, enterprise-grade IBM CICS mainframe emulation system implemented in modern C++20. This library provides faithful emulation of core CICS services for modernization, training, development, and testing purposes.

## Features

### Core CICS Services (35 Modules)

| Module | Description |
|--------|-------------|
| **VSAM Emulation** | KSDS, ESDS, RRDS, LDS file organizations |
| **TSQ Manager** | Temporary Storage Queue operations |
| **TDQ Manager** | Transient Data Queue (intra/extra-partition) |
| **Program Control** | LINK, XCTL, RETURN with COMMAREA |
| **Interval Control** | ASKTIME, DELAY, POST, WAIT, START |
| **Task Control** | ENQ/DEQ resource locking |
| **Storage Control** | GETMAIN/FREEMAIN dynamic storage |
| **Syncpoint Control** | Two-phase commit SYNCPOINT/ROLLBACK |
| **Abend Handler** | HANDLE ABEND, PUSH/POP handler stack |
| **Channel/Container** | Modern data passing between programs |
| **Terminal Control** | SEND/RECEIVE text and control |
| **BMS Support** | Basic Mapping Support map definitions |
| **Spool Control** | JES spool file handling |
| **Document Handler** | DOCUMENT CREATE/INSERT/RETRIEVE |
| **Inquire Services** | INQUIRE PROGRAM/FILE/TRANSACTION |
| **Named Counter** | GET/PUT/UPDATE COUNTER sequences |
| **Journal Control** | WRITE JOURNALNAME audit logging |
| **File Utilities** | Cross-platform file operations |
| **DateTime** | CICS date/time handling |
| **Configuration** | INI/Config file parser |
| **Validation** | Input validation utilities |
| **String Utils** | String manipulation utilities |
| **Serialization** | Binary serialization support |
| **UUID** | UUID generation and parsing |
| **Compression** | Data compression (RLE, LZ77) |

### Utility Modules

| Module | Description |
|--------|-------------|
| **EBCDIC Conversion** | Full ASCII/EBCDIC with packed decimal |
| **Copybook Parser** | COBOL copybook to C++ struct generation |
| **Dump Utilities** | Hex dumps, storage dumps, comparison |
| **JCL Parser** | Job Control Language parsing |
| **GDG Manager** | Generation Data Group support |
| **DFSMShsm** | Hierarchical Storage Manager |
| **Master Catalog** | z/OS catalog emulation |
| **Security** | Authentication and encryption |

## Quick Start

### Building on Linux/macOS

```bash
cd CICS-Emulation-v3.4.6
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run tests
ctest --output-on-failure

# Run examples
./examples/basic/hello-cics
./examples/basic/counter-example
./examples/basic/journal-example
```

### Building on Windows (MinGW)

```batch
cd CICS-Emulation-v3.4.6
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j%NUMBER_OF_PROCESSORS%
```

### Building on Windows (MSVC)

```batch
cd CICS-Emulation-v3.4.6
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## Requirements

| Platform | Compiler | Minimum Version |
|----------|----------|-----------------|
| Windows | MSVC | 2022 (19.29+) |
| Windows | MinGW-w64 | GCC 12+ |
| Linux | GCC | 12+ |
| Linux | Clang | 14+ |
| macOS | Apple Clang | 14+ |

## Usage Examples

### Named Counter (Sequence Generation)

```cpp
#include <cics/counter/counter.hpp>
using namespace cics::counter;

CounterManager::instance().initialize();

// Define counter starting at 1000
exec_cics_define_counter("ORDERNUM", 1000);

// Get next value (returns 1000, increments to 1001)
auto result = exec_cics_get_counter("ORDERNUM");
Int64 order_num = result.value();

CounterManager::instance().shutdown();
```

### Document Handler

```cpp
#include <cics/document/document.hpp>
using namespace cics::document;

DocumentManager::instance().initialize();

auto token = exec_cics_document_create(DocumentType::HTML);
exec_cics_document_set(token.value(), "<h1>Hello, &NAME;</h1>");
exec_cics_document_set_symbol(token.value(), "NAME", "World");
auto content = exec_cics_document_retrieve(token.value());

DocumentManager::instance().shutdown();
```

### Journal Control (Audit Logging)

```cpp
#include <cics/journal/journal.hpp>
using namespace cics::journal;

JournalManager::instance().initialize();

exec_cics_log("Application started");
exec_cics_write_journalname("AUDITLOG", "UPDATE", 
                            "Customer record modified", 24);

JournalManager::instance().shutdown();
```

### VSAM File Operations

```cpp
#include <cics/vsam/vsam_types.hpp>
using namespace cics::vsam;

// Create a KSDS file definition
VsamDefinition def;
def.name = "CUSTOMER.MASTER";
def.type = VsamType::KSDS;
def.key_length = 10;
def.record_length = 200;

// Read a record
auto result = exec_cics_read("CUSTFILE", customer_key);
if (result.is_success()) {
    process_customer(result.value());
}
```

## Documentation

See the `docs/` directory:

| File | Description |
|------|-------------|
| BACKGROUND.md | History of CICS, design philosophy, use cases |
| BUILD.md | Detailed build instructions for all platforms |
| API_REFERENCE.md | Complete API documentation |
| CHANGELOG.md | Version history and changes |
| TESTING.md | Testing guide and examples |
| LICENSE.txt | MIT License text |

## Project Structure

```
CICS-Emulation-v3.4.6/
  apps/
    console-demo/          # Interactive demonstration
  cmake/
    CICSConfig.h.in        # CMake configuration template
  docs/
    *.md, *.txt            # Documentation files
  examples/
    basic/                 # Basic usage examples
    advanced/              # Complete system examples
  libs/
    common/                # Shared utilities
    vsam/                  # VSAM emulation
    tsq/                   # TSQ support
    tdq/                   # TDQ support
    ... (27 modules)
  scripts/
    build.sh               # Linux/macOS build script
    build.bat              # Windows build script
  tests/
    unit/                  # Unit tests
    integration/           # Integration tests
    benchmarks/            # Performance benchmarks
    framework/             # Test framework
  CMakeLists.txt           # Main build configuration
```

## License

This project is licensed under the MIT License - see the LICENSE.txt file for details.

Copyright (c) 2025 Bennie Shearer

## Version History

### 3.4.6 (Current)
- Version synchronization across all modules
- Encoding fixes (Unicode to ASCII-safe)
- Enhanced BACKGROUND.md documentation
- Improved cross-platform compatibility

### 3.4.0
- Document Handler (CREATE/INSERT/RETRIEVE)
- Inquire Services (INQUIRE PROGRAM/FILE/etc.)
- Named Counter (GET/PUT/UPDATE COUNTER)
- Journal Control (WRITE JOURNALNAME)

### 3.3.0
- Syncpoint Control, Abend Handler
- Channel/Container, Terminal Control, Spool Control

### 3.2.0
- Interval Control, Task Control, Storage Control
- Program Control, EBCDIC, BMS, Copybook, Dump

## Author

Bennie Shearer (retired)

## Acknowledgments

Thanks to all my C++ mentors through the years

Special thanks to:

| Organization | Website |
|--------------|---------|
| CICS by IBM | https://www.ibm.com/ |
| CLion by JetBrains s.r.o. | https://www.jetbrains.com/clion/ |
| Claude by Anthropic PBC | https://www.anthropic.com/ |
