cics_add_library(cics-interval-control STATIC
    SOURCES
        src/interval_control.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDENCIES
        cics-common
)
