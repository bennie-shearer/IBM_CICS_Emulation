cics_add_library(cics-storage-control STATIC
    SOURCES
        src/storage_control.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDENCIES
        cics-common
)
