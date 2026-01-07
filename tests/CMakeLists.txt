# Test framework (header-only)
add_library(test-framework INTERFACE)
target_include_directories(test-framework INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/framework)

# NOTE: test_main.cpp is a legacy unity build file not used in CMake builds.
# Individual test targets below are preferred.

# Unit tests - types (only depends on common)
add_executable(test-types unit/test_types.cpp)
target_link_libraries(test-types PRIVATE cics-common test-framework)
target_include_directories(test-types PRIVATE ${PROJECT_SOURCE_DIR}/libs/common/include)
add_test(NAME test_types COMMAND test-types)

# Unit tests - error (only depends on common)
add_executable(test-error unit/test_error.cpp)
target_link_libraries(test-error PRIVATE cics-common test-framework)
target_include_directories(test-error PRIVATE ${PROJECT_SOURCE_DIR}/libs/common/include)
add_test(NAME test_error COMMAND test-error)

# Unit tests - vsam
add_executable(test-vsam unit/test_vsam.cpp)
target_link_libraries(test-vsam PRIVATE cics-common cics-vsam test-framework)
target_include_directories(test-vsam PRIVATE 
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/vsam/include)
add_test(NAME test_vsam COMMAND test-vsam)

# Unit tests - cics
add_executable(test-cics unit/test_cics.cpp)
target_link_libraries(test-cics PRIVATE cics-common cics-cics-core test-framework)
target_include_directories(test-cics PRIVATE 
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/cics-core/include)
add_test(NAME test_cics COMMAND test-cics)

# Integration tests
add_executable(test-vsam-integration integration/test_vsam_integration.cpp)
target_link_libraries(test-vsam-integration PRIVATE 
    cics-common cics-vsam cics-master-catalog test-framework)
target_include_directories(test-vsam-integration PRIVATE 
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/vsam/include
    ${PROJECT_SOURCE_DIR}/libs/master-catalog/include)
add_test(NAME test_vsam_integration COMMAND test-vsam-integration)

# Benchmarks
if(CICS_BUILD_BENCHMARKS)
    # Main VSAM benchmark
    add_executable(benchmark-vsam benchmarks/benchmark_vsam.cpp)
    target_link_libraries(benchmark-vsam PRIVATE cics-common cics-vsam)
    target_include_directories(benchmark-vsam PRIVATE 
        ${PROJECT_SOURCE_DIR}/libs/common/include
        ${PROJECT_SOURCE_DIR}/libs/vsam/include
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
    
    # Alternative benchmark main (uses benchmark_main.cpp)
    add_executable(benchmark-main benchmarks/benchmark_main.cpp)
    target_link_libraries(benchmark-main PRIVATE cics-common cics-vsam)
    target_include_directories(benchmark-main PRIVATE 
        ${PROJECT_SOURCE_DIR}/libs/common/include
        ${PROJECT_SOURCE_DIR}/libs/vsam/include
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
endif()

if(WIN32)
    target_compile_definitions(test-types PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(test-error PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(test-vsam PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(test-cics PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(test-vsam-integration PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(CICS_BUILD_BENCHMARKS)
        target_compile_definitions(benchmark-vsam PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_definitions(benchmark-main PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
endif()
