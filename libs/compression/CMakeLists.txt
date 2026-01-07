# =============================================================================
# IBM CICS Emulation - Compression Module
# Version: 3.4.6
# =============================================================================

cmake_minimum_required(VERSION 3.20)

add_library(cics-compression
    src/compression.cpp
)

add_library(cics::compression ALIAS cics-compression)

target_include_directories(cics-compression
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cics-compression
    PUBLIC
        cics::common
)

target_compile_features(cics-compression PUBLIC cxx_std_20)

if(WIN32)
    target_compile_definitions(cics-compression PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(cics-compression PROPERTIES
    VERSION 3.4.6
    SOVERSION 3
    EXPORT_NAME compression
)
