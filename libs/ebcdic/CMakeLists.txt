cics_add_library(cics-ebcdic STATIC
    SOURCES
        src/ebcdic.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDENCIES
        cics-common
)
