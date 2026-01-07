add_library(cics-terminal-control
    src/terminal.cpp
)

target_include_directories(cics-terminal-control PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-terminal-control PUBLIC cics-common)
