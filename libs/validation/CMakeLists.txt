# =============================================================================
# IBM CICS Emulation - Validation Module
# Version: 3.4.6
# =============================================================================

cmake_minimum_required(VERSION 3.20)

add_library(cics-validation
    src/validation.cpp
)

add_library(cics::validation ALIAS cics-validation)

target_include_directories(cics-validation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cics-validation
    PUBLIC
        cics::common
)

target_compile_features(cics-validation PUBLIC cxx_std_20)

if(WIN32)
    target_compile_definitions(cics-validation PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(cics-validation PROPERTIES
    VERSION 3.4.6
    SOVERSION 3
    EXPORT_NAME validation
)
