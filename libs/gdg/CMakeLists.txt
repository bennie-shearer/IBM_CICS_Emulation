add_library(cics-gdg STATIC
    src/gdg_manager.cpp
    src/generation.cpp
)

target_include_directories(cics-gdg PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-gdg PUBLIC cics-common)
