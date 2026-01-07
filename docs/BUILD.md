# IBM CICS (Customer Information Control System) Emulation - Build Instructions
Version 3.4.6

This document provides comprehensive build instructions for Windows, Linux, and macOS.

## Prerequisites

### Required Software

| Component | Minimum Version | Recommended |
|-----------|-----------------|-------------|
| CMake | 3.20 | 3.25+ |
| C++ Compiler | C++20 support | GCC 12+, Clang 14+, MSVC 2022 |
| Threading | pthreads (POSIX) or Win32 threads | - |

### Platform-Specific Requirements

#### Windows

**Option 1: Visual Studio**
- Visual Studio 2019 (16.10+) or Visual Studio 2022
- "Desktop development with C++" workload
- CMake tools for Windows

**Option 2: MinGW-w64**
- MinGW-w64 with GCC 10 or higher
- MSYS2 recommended for package management

```bash
# MSYS2 installation
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make
```

#### Linux

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake g++-12

# Fedora/RHEL
sudo dnf install gcc-c++ cmake make

# Arch Linux
sudo pacman -S base-devel cmake
```

#### macOS

```bash
# Using Homebrew
brew install cmake

# Xcode Command Line Tools (required)
xcode-select --install
```

## Build Commands

### Quick Build (All Platforms)

```bash
# Clone or extract the source
cd CICS-Emulation-v3.4.6

# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build (parallel)
cmake --build . --parallel

# Optional: Install
cmake --install . --prefix /usr/local
```

### Platform-Specific Commands

#### Windows (Visual Studio)

```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release --parallel
```

#### Windows (MinGW)

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j%NUMBER_OF_PROCESSORS%
```

#### Linux (GCC)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
make -j$(nproc)
```

#### Linux (Clang)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
make -j$(nproc)
```

#### macOS

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Debug | Build type: Debug, Release, RelWithDebInfo, MinSizeRel |
| `CICS_BUILD_TESTS` | ON | Build unit and integration tests |
| `CICS_BUILD_EXAMPLES` | ON | Build example programs |
| `CICS_BUILD_BENCHMARKS` | ON | Build performance benchmarks |
| `CICS_ENABLE_SIMD` | ON | Enable SIMD optimizations where available |
| `CICS_ENABLE_THREADING` | ON | Enable multi-threading support |
| `CICS_ENABLE_ENCRYPTION` | ON | Enable encryption features |
| `CICS_ENABLE_LOGGING` | ON | Enable logging subsystem |

### Example with Options

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCICS_BUILD_TESTS=OFF \
    -DCICS_BUILD_EXAMPLES=ON \
    -DCICS_ENABLE_SIMD=ON
```

## Build Output

### Libraries (in `build/lib/`)

| Library | Description |
|---------|-------------|
| libcics-common.a | Core types, error handling, logging, threading |
| libcics-security.a | Authentication and encryption |
| libcics-master-catalog.a | z/OS catalog emulation |
| libcics-vsam.a | VSAM file support (KSDS, RRDS, ESDS, LDS) |
| libcics-dfsmshsm.a | Hierarchical Storage Management |
| libcics-cics-core.a | Transaction and file control |
| libcics-gdg.a | Generation Data Groups |
| libcics-tsq.a | Temporary Storage Queues |
| libcics-tdq.a | Transient Data Queues |
| libcics-jcl.a | JCL Parser |
| libcics-interval-control.a | Time and scheduling (NEW) |
| libcics-task-control.a | Resource serialization (NEW) |
| libcics-storage-control.a | Dynamic storage (NEW) |
| libcics-program-control.a | Program invocation (NEW) |
| libcics-ebcdic.a | Character conversion (NEW) |
| libcics-bms.a | Basic Mapping Support (NEW) |
| libcics-copybook.a | Copybook parser (NEW) |
| libcics-dump.a | Dump utilities (NEW) |

### Example Programs (in `build/examples/basic/`)

| Executable | Description |
|------------|-------------|
| hello-cics | Simple CICS introduction |
| vsam-example | VSAM file operations |
| cics-example | Core CICS functionality |
| tsq-example | Temporary Storage Queue demo |
| tdq-example | Transient Data Queue demo |
| jcl-example | JCL parsing demo |
| interval-example | Interval control demo (NEW) |
| task-example | Task control demo (NEW) |
| storage-example | Storage control demo (NEW) |
| ebcdic-example | EBCDIC conversion demo (NEW) |
| dump-example | Dump utilities demo (NEW) |

## Troubleshooting

### Common Issues

#### "C++20 features not supported"

Ensure your compiler supports C++20:
```bash
# Check GCC version
g++ --version  # Should be 10+

# Check Clang version
clang++ --version  # Should be 12+
```

#### "pthread not found" (Linux)

```bash
sudo apt install libpthread-stubs0-dev
```

#### Windows macro conflicts

The build system automatically defines `NOMINMAX` and `WIN32_LEAN_AND_MEAN` to prevent Windows header conflicts.

#### MinGW linking errors

Ensure MinGW-w64 is properly installed:
```bash
# In MSYS2
pacman -S mingw-w64-x86_64-toolchain
```

### Build Verification

After building, run the examples to verify:

```bash
cd build

# Run interval control example
./examples/basic/interval-example

# Run EBCDIC conversion example
./examples/basic/ebcdic-example

# Run dump utilities example
./examples/basic/dump-example
```

## Integration

### Using in Your Project (CMake)

```cmake
# Find the installed package
find_package(CICSEmulation REQUIRED)

# Link your executable
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE 
    cics::common
    cics::interval-control
    cics::ebcdic
)
```

### Manual Linking

```bash
g++ -std=c++20 myapp.cpp \
    -I/path/to/cics/include \
    -L/path/to/cics/lib \
    -lcics-interval-control -lcics-ebcdic -lcics-common \
    -lpthread -o myapp
```

## Support

For build issues, please check:
1. Compiler version meets C++20 requirements
2. All dependencies are installed
3. CMake version is 3.20 or higher

Copyright (c) 2025 Bennie Shearer
