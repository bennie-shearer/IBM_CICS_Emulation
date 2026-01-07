add_library(cics-journal
    src/journal.cpp
)

target_include_directories(cics-journal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-journal PUBLIC cics-common)
