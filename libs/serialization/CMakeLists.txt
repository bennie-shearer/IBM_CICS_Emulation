# =============================================================================
# IBM CICS Emulation - Serialization Module
# Version: 3.4.6
# =============================================================================

cmake_minimum_required(VERSION 3.20)

add_library(cics-serialization
    src/serialization.cpp
)

add_library(cics::serialization ALIAS cics-serialization)

target_include_directories(cics-serialization
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cics-serialization
    PUBLIC
        cics::common
)

target_compile_features(cics-serialization PUBLIC cxx_std_20)

if(WIN32)
    target_compile_definitions(cics-serialization PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(cics-serialization PROPERTIES
    VERSION 3.4.6
    SOVERSION 3
    EXPORT_NAME serialization
)
