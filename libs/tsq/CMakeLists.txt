cics_add_library(cics-tsq STATIC
    SOURCES
        src/tsq_manager.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDENCIES
        cics-common
)
