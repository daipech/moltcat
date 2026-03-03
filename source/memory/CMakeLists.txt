# Memory management library
add_library(moltcat_memory STATIC
    memory_manager.cpp
    agent_memory_view.cpp
)

target_include_directories(moltcat_memory
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_memory
    PUBLIC
        moltcat_model
        moltcat_utils
)
