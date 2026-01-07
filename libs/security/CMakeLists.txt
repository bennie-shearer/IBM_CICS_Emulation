add_library(cics-security STATIC
    src/authentication.cpp
    src/encryption.cpp
    src/security_context.cpp
)

target_include_directories(cics-security PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cics-security PUBLIC cics-common)
