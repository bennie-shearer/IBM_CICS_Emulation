add_library(cics-spool
    src/spool.cpp
)

target_include_directories(cics-spool PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-spool PUBLIC cics-common)
