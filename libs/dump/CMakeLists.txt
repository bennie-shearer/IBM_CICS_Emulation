cics_add_library(cics-dump STATIC
    SOURCES
        src/dump.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDENCIES
        cics-common
)
