# =============================================================================
# IBM CICS Emulation - Configuration Module
# Version: 3.4.6
# =============================================================================

cmake_minimum_required(VERSION 3.20)

add_library(cics-config
    src/config.cpp
)

add_library(cics::config ALIAS cics-config)

target_include_directories(cics-config
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cics-config
    PUBLIC
        cics::common
)

target_compile_features(cics-config PUBLIC cxx_std_20)

if(WIN32)
    target_compile_definitions(cics-config PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(cics-config PROPERTIES
    VERSION 3.4.6
    SOVERSION 3
    EXPORT_NAME config
)
