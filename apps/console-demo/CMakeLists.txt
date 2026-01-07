add_executable(cics-demo main.cpp)

target_include_directories(cics-demo PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/security/include
    ${PROJECT_SOURCE_DIR}/libs/master-catalog/include
    ${PROJECT_SOURCE_DIR}/libs/vsam/include
    ${PROJECT_SOURCE_DIR}/libs/dfsmshsm/include
    ${PROJECT_SOURCE_DIR}/libs/cics-core/include
    ${PROJECT_SOURCE_DIR}/libs/gdg/include
)

target_link_libraries(cics-demo PRIVATE
    cics-common
    cics-security
    cics-master-catalog
    cics-vsam
    cics-dfsmshsm
    cics-cics-core
    cics-gdg
)

if(WIN32)
    target_compile_definitions(cics-demo PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
