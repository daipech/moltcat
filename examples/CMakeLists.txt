# MoltCat Examples Configuration

# ================================
# Example skill plugins
# ================================

# Example echo skill plugin
add_library(example_skill SHARED
    example_skill.cpp
)

target_include_directories(example_skill
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(example_skill
    PRIVATE
        moltcat_core
)

# Set output directory
set_target_properties(example_skill PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/plugins
)

# Calculator skill plugin
add_subdirectory(calculator_skill)

# ================================
# Example programs
# ================================

# Simple agent example (standalone, not a plugin)
add_executable(simple_agent
    simple_agent.cpp
)

target_link_libraries(simple_agent PRIVATE moltcat_core)

set_target_properties(simple_agent PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

# Threading model example
add_executable(threading_example
    threading_example.cpp
)

target_link_libraries(threading_example PRIVATE moltcat_core uv::uv_a)

set_target_properties(threading_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

# Hybrid executor example
add_executable(hybrid_executor_example
    hybrid_executor_example.cpp
)

target_link_libraries(hybrid_executor_example PRIVATE moltcat_core uv::uv_a)

set_target_properties(hybrid_executor_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

# Unified executor interface example
add_executable(unified_executor_example
    unified_executor_example.cpp
)

target_link_libraries(unified_executor_example PRIVATE moltcat_core uv::uv_a)

set_target_properties(unified_executor_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

# Unified scheduler interface example
add_executable(scheduler_example
    scheduler_example.cpp
)

target_link_libraries(scheduler_example PRIVATE moltcat_core uv::uv_a)

set_target_properties(scheduler_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)
