add_library(cics-document
    src/document.cpp
)

target_include_directories(cics-document PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-document PUBLIC cics-common)
