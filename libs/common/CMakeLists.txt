add_library(cics-common STATIC
    src/types.cpp
    src/error.cpp
    src/logging.cpp
    src/threading.cpp
)

add_library(cics::common ALIAS cics-common)

target_include_directories(cics-common PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-common PUBLIC Threads::Threads)

set_target_properties(cics-common PROPERTIES
    OUTPUT_NAME "cics-common"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
