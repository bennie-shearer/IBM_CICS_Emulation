cics_add_library(cics-bms STATIC
    SOURCES
        src/bms.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDENCIES
        cics-common
)
