# =============================================================================
# IBM CICS Emulation - File Utilities Module
# Version: 3.4.6
# =============================================================================

cmake_minimum_required(VERSION 3.20)

add_library(cics-file-utils
    src/file_utils.cpp
)

add_library(cics::file-utils ALIAS cics-file-utils)

target_include_directories(cics-file-utils
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cics-file-utils
    PUBLIC
        cics::common
)

target_compile_features(cics-file-utils PUBLIC cxx_std_20)

if(WIN32)
    target_compile_definitions(cics-file-utils PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(cics-file-utils PROPERTIES
    VERSION 3.4.6
    SOVERSION 3
    EXPORT_NAME file-utils
)
