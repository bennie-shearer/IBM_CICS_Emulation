cmake_minimum_required(VERSION 3.20)

# =============================================================================
# CICS Emulation - Build Configuration
# Version: 3.4.6 | January 2025
# =============================================================================

project(IBM_CICS_Emulation
    VERSION 3.4.6
    DESCRIPTION "IBM CICS Emulation"
    LANGUAGES CXX
)

# =============================================================================
# Build Options
# =============================================================================

option(CICS_BUILD_TESTS "Build test suite" ON)
option(CICS_BUILD_EXAMPLES "Build example applications" ON)
option(CICS_BUILD_BENCHMARKS "Build benchmark suite" ON)
option(CICS_ENABLE_SIMD "Enable SIMD optimizations" ON)
option(CICS_ENABLE_THREADING "Enable multi-threading support" ON)
option(CICS_ENABLE_ENCRYPTION "Enable encryption features" ON)
option(CICS_ENABLE_LOGGING "Enable logging framework" ON)
option(CICS_ENABLE_PROFILING "Enable profiling support" OFF)
option(CICS_ENABLE_COVERAGE "Enable code coverage" OFF)
option(CICS_STRICT_WARNINGS "Enable strict compiler warnings" ON)

# =============================================================================
# C++ Standard and Global Settings
# =============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# =============================================================================
# Platform Detection and Configuration
# =============================================================================

if(WIN32)
    set(CICS_PLATFORM "Windows")
    set(CICS_PLATFORM_WINDOWS TRUE)
    add_compile_definitions(CICS_PLATFORM_WINDOWS=1)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
elseif(APPLE)
    set(CICS_PLATFORM "macOS")
    set(CICS_PLATFORM_MACOS TRUE)
    add_compile_definitions(CICS_PLATFORM_MACOS=1)
elseif(UNIX)
    set(CICS_PLATFORM "Linux")
    set(CICS_PLATFORM_LINUX TRUE)
    add_compile_definitions(CICS_PLATFORM_LINUX=1)
else()
    message(WARNING "Unknown platform")
    set(CICS_PLATFORM "Unknown")
endif()

# =============================================================================
# Compiler Configuration
# =============================================================================

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
        message(FATAL_ERROR "GCC 10.0+ required for C++20 support")
    endif()
    
    # Detect MinGW
    if(MINGW)
        set(CICS_MINGW TRUE)
        message(STATUS "MinGW detected - enabling compatibility options")
        add_compile_definitions(CICS_MINGW=1)
        # MinGW-specific flags
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32_WINNT=0x0601")
        # Disable some warnings that are overly aggressive in MinGW
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overread")
    endif()
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    
    if(CICS_STRICT_WARNINGS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Wconversion -Wshadow")
        if(NOT MINGW)
            # These warnings can be problematic on MinGW with older headers
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wstringop-overread")
        endif()
    endif()
    
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DCICS_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    if(NOT MINGW)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
    else()
        # MinGW: Use a more portable optimization level
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mtune=generic")
    endif()
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
    
    if(CICS_ENABLE_SIMD)
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
        if(COMPILER_SUPPORTS_AVX2 AND NOT MINGW)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
        elseif(MINGW)
            # MinGW: Check SSE2 as a safer baseline
            check_cxx_compiler_flag("-msse2" COMPILER_SUPPORTS_SSE2)
            if(COMPILER_SUPPORTS_SSE2)
                set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
            endif()
        endif()
    endif()
    
    if(CICS_ENABLE_COVERAGE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12.0")
        message(FATAL_ERROR "Clang 12.0+ required for C++20 support")
    endif()
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    
    if(CICS_STRICT_WARNINGS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Wconversion")
    endif()
    
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DCICS_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

elseif(MSVC)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.29")
        message(FATAL_ERROR "MSVC 19.29+ required (Visual Studio 2019 16.10+)")
    endif()
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /permissive- /Zc:__cplusplus /utf-8")
    
    if(CICS_STRICT_WARNINGS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
    endif()
    
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /RTC1 /DCICS_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /GL /DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    
    if(CICS_ENABLE_SIMD)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
    endif()
endif()

# =============================================================================
# Dependencies
# =============================================================================

find_package(Threads REQUIRED)

# Optional OpenSSL for encryption
if(CICS_ENABLE_ENCRYPTION)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
        add_compile_definitions(CICS_HAS_OPENSSL=1)
    else()
        message(STATUS "OpenSSL not found - using built-in crypto")
    endif()
endif()

# =============================================================================
# Helper Functions
# =============================================================================

function(cics_add_library target_name)
    cmake_parse_arguments(ARG "STATIC;SHARED" "" "SOURCES;HEADERS;INCLUDE_DIRS;DEPENDENCIES" ${ARGN})
    
    if(ARG_STATIC)
        add_library(${target_name} STATIC ${ARG_SOURCES})
    elseif(ARG_SHARED)
        add_library(${target_name} SHARED ${ARG_SOURCES})
    else()
        add_library(${target_name} STATIC ${ARG_SOURCES})
    endif()
    
    target_include_directories(${target_name} PUBLIC ${ARG_INCLUDE_DIRS})
    
    if(ARG_DEPENDENCIES)
        target_link_libraries(${target_name} PUBLIC ${ARG_DEPENDENCIES})
    endif()
    
    # Set output directories
    set_target_properties(${target_name} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
    
    add_library(cics::${target_name} ALIAS ${target_name})
endfunction()

function(cics_add_executable target_name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDENCIES" ${ARGN})
    
    add_executable(${target_name} ${ARG_SOURCES})
    
    if(ARG_DEPENDENCIES)
        target_link_libraries(${target_name} PRIVATE ${ARG_DEPENDENCIES})
    endif()
    
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endfunction()

# =============================================================================
# Configuration Header
# =============================================================================

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CICSConfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/cics/config.h"
    @ONLY
)

include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")

# =============================================================================
# Libraries (dependency order)
# =============================================================================

# Core libraries
add_subdirectory(libs/common)
add_subdirectory(libs/security)
add_subdirectory(libs/master-catalog)
add_subdirectory(libs/vsam)
add_subdirectory(libs/dfsmshsm)
add_subdirectory(libs/cics-core)
add_subdirectory(libs/gdg)

# Queue modules
add_subdirectory(libs/tsq)
add_subdirectory(libs/tdq)

# Parser modules
add_subdirectory(libs/jcl)
add_subdirectory(libs/copybook)

# Control modules (new in v3.2.0)
add_subdirectory(libs/interval-control)
add_subdirectory(libs/task-control)
add_subdirectory(libs/storage-control)
add_subdirectory(libs/program-control)

# Utility modules (new in v3.2.0)
add_subdirectory(libs/ebcdic)
add_subdirectory(libs/bms)
add_subdirectory(libs/dump)

# Additional modules (new in v3.3.0)
add_subdirectory(libs/syncpoint)
add_subdirectory(libs/abend-handler)
add_subdirectory(libs/channel)
add_subdirectory(libs/terminal-control)
add_subdirectory(libs/spool)

# Additional modules (new in v3.4.0)
add_subdirectory(libs/document)
add_subdirectory(libs/inquire)
add_subdirectory(libs/named-counter)
add_subdirectory(libs/journal)

# New utility modules (v3.4.6)
add_subdirectory(libs/file-utils)
add_subdirectory(libs/datetime)
add_subdirectory(libs/config)
add_subdirectory(libs/validation)
add_subdirectory(libs/string-utils)
add_subdirectory(libs/serialization)
add_subdirectory(libs/uuid)
add_subdirectory(libs/compression)

# =============================================================================
# Applications
# =============================================================================

add_subdirectory(apps/console-demo)

# =============================================================================
# Tests
# =============================================================================

if(CICS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# Examples
# =============================================================================

if(CICS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# =============================================================================
# Installation
# =============================================================================

include(GNUInstallDirs)

# Install headers
foreach(lib common security master-catalog vsam dfsmshsm cics-core gdg tsq tdq jcl
             copybook interval-control task-control storage-control program-control
             ebcdic bms dump syncpoint abend-handler channel terminal-control spool
             document inquire named-counter journal file-utils datetime config validation
             string-utils serialization uuid compression)
    install(DIRECTORY libs/${lib}/include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.hpp")
endforeach()

# Install binaries
install(TARGETS cics-demo
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# =============================================================================
# Package Configuration
# =============================================================================

set(CPACK_PACKAGE_NAME "IBM_CICS_Emulation")
set(CPACK_PACKAGE_VENDOR "Bennie Shearer")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)

# =============================================================================
# Build Summary
# =============================================================================

message(STATUS "")
message(STATUS "+==============================================================+")
message(STATUS "|  CICS Emulation Enterprise v${PROJECT_VERSION} - Build Configuration  |")
message(STATUS "+==============================================================+")
message(STATUS "|  Platform:     ${CICS_PLATFORM}")
message(STATUS "|  Compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "|  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "|  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "+==============================================================+")
message(STATUS "|  Features:")
message(STATUS "|    Tests:       ${CICS_BUILD_TESTS}")
message(STATUS "|    Examples:    ${CICS_BUILD_EXAMPLES}")
message(STATUS "|    Benchmarks:  ${CICS_BUILD_BENCHMARKS}")
message(STATUS "|    SIMD:        ${CICS_ENABLE_SIMD}")
message(STATUS "|    Threading:   ${CICS_ENABLE_THREADING}")
message(STATUS "|    Encryption:  ${CICS_ENABLE_ENCRYPTION}")
message(STATUS "|    Logging:     ${CICS_ENABLE_LOGGING}")
message(STATUS "+==============================================================+")
message(STATUS "")
