cics_add_library(cics-copybook STATIC
    SOURCES
        src/copybook.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDENCIES
        cics-common
        cics-ebcdic
)
