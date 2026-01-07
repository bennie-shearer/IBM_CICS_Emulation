#!/bin/bash
# =============================================================================
# CICS Emulation Enterprise - Build Script for Linux/macOS
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
BUILD_TYPE="${1:-Release}"

echo "+==============================================================+"
echo "|       CICS Emulation Enterprise v3.1.6 - Build Script        |"
echo "+==============================================================+"
echo ""
echo "Project root: ${PROJECT_ROOT}"
echo "Build type: ${BUILD_TYPE}"
echo ""

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "Error: CMake not found. Please install CMake 3.20+"
    exit 1
fi

# Check CMake version
CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
echo "CMake version: ${CMAKE_VERSION}"

# Check compiler
if command -v g++ &> /dev/null; then
    GCC_VERSION=$(g++ --version | head -n1)
    echo "Compiler: ${GCC_VERSION}"
elif command -v clang++ &> /dev/null; then
    CLANG_VERSION=$(clang++ --version | head -n1)
    echo "Compiler: ${CLANG_VERSION}"
fi

echo ""

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure
echo "Configuring..."
cmake "${PROJECT_ROOT}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCICS_BUILD_TESTS=ON \
    -DCICS_BUILD_EXAMPLES=ON \
    -DCICS_BUILD_BENCHMARKS=ON

# Build
echo ""
echo "Building..."
cmake --build . --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "+==============================================================+"
echo "|                     Build Complete!                          |"
echo "+==============================================================+"
echo ""
echo "Binaries are in: ${BUILD_DIR}/bin"
echo ""
echo "To run tests:    cd ${BUILD_DIR} && ctest --output-on-failure"
echo "To run demo:     ${BUILD_DIR}/bin/cics-demo"
