# Complete System Example
add_executable(complete-system complete_system.cpp)
target_include_directories(complete-system PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/security/include
    ${PROJECT_SOURCE_DIR}/libs/master-catalog/include
    ${PROJECT_SOURCE_DIR}/libs/vsam/include
    ${PROJECT_SOURCE_DIR}/libs/cics-core/include
    ${PROJECT_SOURCE_DIR}/libs/gdg/include
    ${PROJECT_SOURCE_DIR}/libs/dfsmshsm/include
)
target_link_libraries(complete-system PRIVATE 
    cics-common 
    cics-security 
    cics-master-catalog 
    cics-vsam 
    cics-cics-core
    cics-gdg
    cics-dfsmshsm
)

if(WIN32)
    target_compile_definitions(complete-system PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Note: transaction_processor.cpp is provided as a template example
# It requires namespace adaptations for your specific use case
