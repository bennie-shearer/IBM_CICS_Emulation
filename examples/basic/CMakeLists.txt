# VSAM Example
add_executable(vsam-example vsam_example.cpp)
target_include_directories(vsam-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/vsam/include
)
target_link_libraries(vsam-example PRIVATE cics-common cics-vsam)

# CICS Example
add_executable(cics-example cics_example.cpp)
target_include_directories(cics-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/cics-core/include
)
target_link_libraries(cics-example PRIVATE cics-common cics-cics-core)

# Hello CICS Example
add_executable(hello-cics hello_cics.cpp)
target_include_directories(hello-cics PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/cics-core/include
)
target_link_libraries(hello-cics PRIVATE cics-common cics-cics-core)

# TSQ Example
add_executable(tsq-example tsq_example.cpp)
target_include_directories(tsq-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/tsq/include
)
target_link_libraries(tsq-example PRIVATE cics-common cics-tsq)

# TDQ Example
add_executable(tdq-example tdq_example.cpp)
target_include_directories(tdq-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/tdq/include
)
target_link_libraries(tdq-example PRIVATE cics-common cics-tdq)

# JCL Parser Example
add_executable(jcl-example jcl_example.cpp)
target_include_directories(jcl-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/jcl/include
)
target_link_libraries(jcl-example PRIVATE cics-common cics-jcl)

# Interval Control Example (new in v3.2.0)
add_executable(interval-example interval_example.cpp)
target_include_directories(interval-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/interval-control/include
)
target_link_libraries(interval-example PRIVATE cics-common cics-interval-control Threads::Threads)

# Task Control Example (new in v3.2.0)
add_executable(task-example task_example.cpp)
target_include_directories(task-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/task-control/include
)
target_link_libraries(task-example PRIVATE cics-common cics-task-control Threads::Threads)

# Storage Control Example (new in v3.2.0)
add_executable(storage-example storage_example.cpp)
target_include_directories(storage-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/storage-control/include
)
target_link_libraries(storage-example PRIVATE cics-common cics-storage-control)

# EBCDIC Conversion Example (new in v3.2.0)
add_executable(ebcdic-example ebcdic_example.cpp)
target_include_directories(ebcdic-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/ebcdic/include
)
target_link_libraries(ebcdic-example PRIVATE cics-common cics-ebcdic)

# Dump Utilities Example (new in v3.2.0)
add_executable(dump-example dump_example.cpp)
target_include_directories(dump-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/dump/include
)
target_link_libraries(dump-example PRIVATE cics-common cics-dump)

# Syncpoint Control Example (new in v3.3.0)
add_executable(syncpoint-example syncpoint_example.cpp)
target_include_directories(syncpoint-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/syncpoint/include
)
target_link_libraries(syncpoint-example PRIVATE cics-common cics-syncpoint Threads::Threads)

# Channel/Container Example (new in v3.3.0)
add_executable(channel-example channel_example.cpp)
target_include_directories(channel-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/channel/include
)
target_link_libraries(channel-example PRIVATE cics-common cics-channel)

# Spool Control Example (new in v3.3.0)
add_executable(spool-example spool_example.cpp)
target_include_directories(spool-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/spool/include
)
target_link_libraries(spool-example PRIVATE cics-common cics-spool)

# Document Handler Example (new in v3.4.6)
add_executable(document-example document_example.cpp)
target_include_directories(document-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/document/include
)
target_link_libraries(document-example PRIVATE cics-common cics-document)

# Named Counter Example (new in v3.4.6)
add_executable(counter-example counter_example.cpp)
target_include_directories(counter-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/named-counter/include
)
target_link_libraries(counter-example PRIVATE cics-common cics-named-counter)

# Journal Control Example (new in v3.4.6)
add_executable(journal-example journal_example.cpp)
target_include_directories(journal-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
    ${PROJECT_SOURCE_DIR}/libs/journal/include
)
target_link_libraries(journal-example PRIVATE cics-common cics-journal)

# Utilities Example (new in v3.4.6)
add_executable(utilities-example utilities_example.cpp)
target_include_directories(utilities-example PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/common/include
)
target_link_libraries(utilities-example PRIVATE cics-common)

if(WIN32)
    target_compile_definitions(vsam-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(cics-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(hello-cics PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(tsq-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(tdq-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(jcl-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(interval-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(task-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(storage-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(ebcdic-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(dump-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(syncpoint-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(channel-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(spool-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(document-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(counter-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(journal-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(utilities-example PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
