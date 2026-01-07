cics_add_library(cics-jcl STATIC
    SOURCES
        src/jcl_parser.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDENCIES
        cics-common
)
