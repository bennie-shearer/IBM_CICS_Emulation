add_library(cics-dfsmshsm STATIC
    src/migration_manager.cpp
    src/storage_manager.cpp
)

target_include_directories(cics-dfsmshsm PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-dfsmshsm PUBLIC cics-common)
