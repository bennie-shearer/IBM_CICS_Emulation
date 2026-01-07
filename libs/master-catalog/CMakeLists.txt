add_library(cics-master-catalog STATIC
    src/catalog_entry.cpp
    src/master_catalog.cpp
    src/volume_manager.cpp
)

target_include_directories(cics-master-catalog PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-master-catalog PUBLIC cics-common)
