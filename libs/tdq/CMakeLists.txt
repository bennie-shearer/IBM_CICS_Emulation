cics_add_library(cics-tdq STATIC
    SOURCES
        src/tdq_manager.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDENCIES
        cics-common
)
