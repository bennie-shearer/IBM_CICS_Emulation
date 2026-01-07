add_library(cics-channel
    src/channel.cpp
)

target_include_directories(cics-channel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-channel PUBLIC cics-common)
