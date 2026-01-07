add_library(cics-named-counter
    src/counter.cpp
)

target_include_directories(cics-named-counter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-named-counter PUBLIC cics-common)
