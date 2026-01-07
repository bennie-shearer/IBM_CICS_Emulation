cics_add_library(cics-program-control STATIC
    SOURCES
        src/program_control.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDENCIES
        cics-common
)
