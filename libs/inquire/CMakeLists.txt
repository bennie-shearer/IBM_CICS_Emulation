add_library(cics-inquire
    src/inquire.cpp
)

target_include_directories(cics-inquire PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-inquire PUBLIC cics-common)
