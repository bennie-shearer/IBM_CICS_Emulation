# =============================================================================
# IBM CICS Emulation - DateTime Utilities Module
# Version: 3.4.6
# =============================================================================

cmake_minimum_required(VERSION 3.20)

add_library(cics-datetime
    src/datetime.cpp
)

add_library(cics::datetime ALIAS cics-datetime)

target_include_directories(cics-datetime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cics-datetime
    PUBLIC
        cics::common
)

target_compile_features(cics-datetime PUBLIC cxx_std_20)

if(WIN32)
    target_compile_definitions(cics-datetime PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(cics-datetime PROPERTIES
    VERSION 3.4.6
    SOVERSION 3
    EXPORT_NAME datetime
)
