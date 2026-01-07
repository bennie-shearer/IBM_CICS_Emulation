add_library(cics-abend-handler
    src/abend.cpp
)

target_include_directories(cics-abend-handler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-abend-handler PUBLIC cics-common)
