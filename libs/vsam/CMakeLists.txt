add_library(cics-vsam STATIC
    src/ksds_file.cpp
    src/vsam_buffer.cpp
    src/vsam_file.cpp
    src/vsam_index.cpp
)

target_include_directories(cics-vsam PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-vsam PUBLIC cics-common)
