add_library(cics-syncpoint
    src/syncpoint.cpp
)

target_include_directories(cics-syncpoint PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-syncpoint PUBLIC cics-common)
