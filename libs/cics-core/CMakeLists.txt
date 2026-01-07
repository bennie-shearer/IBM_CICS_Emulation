add_library(cics-cics-core STATIC
    src/cics_manager.cpp
    src/file_control.cpp
    src/program_manager.cpp
    src/transaction_manager.cpp
)

target_include_directories(cics-cics-core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-cics-core PUBLIC cics-common)
