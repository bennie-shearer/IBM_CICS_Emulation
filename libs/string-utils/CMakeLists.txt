# =============================================================================
# IBM CICS Emulation - String Utilities Module
# Version: 3.4.6
# =============================================================================

cmake_minimum_required(VERSION 3.20)

add_library(cics-string-utils
    src/string_utils.cpp
)

add_library(cics::string-utils ALIAS cics-string-utils)

target_include_directories(cics-string-utils
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cics-string-utils
    PUBLIC
        cics::common
)

target_compile_features(cics-string-utils PUBLIC cxx_std_20)

if(WIN32)
    target_compile_definitions(cics-string-utils PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(cics-string-utils PROPERTIES
    VERSION 3.4.6
    SOVERSION 3
    EXPORT_NAME string-utils
)
