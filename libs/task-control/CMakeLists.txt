cics_add_library(cics-task-control STATIC
    SOURCES
        src/task_control.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    DEPENDENCIES
        cics-common
)
