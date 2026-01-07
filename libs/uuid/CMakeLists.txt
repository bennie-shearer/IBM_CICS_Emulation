# =============================================================================
# IBM CICS Emulation - UUID Module
# Version: 3.4.6
# =============================================================================

cmake_minimum_required(VERSION 3.20)

add_library(cics-uuid
    src/uuid.cpp
)

add_library(cics::uuid ALIAS cics-uuid)

target_include_directories(cics-uuid
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cics-uuid
    PUBLIC
        cics::common
)

target_compile_features(cics-uuid PUBLIC cxx_std_20)

if(WIN32)
    target_compile_definitions(cics-uuid PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(cics-uuid PROPERTIES
    VERSION 3.4.6
    SOVERSION 3
    EXPORT_NAME uuid
)
