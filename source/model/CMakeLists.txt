# Data model library
add_library(moltcat_model STATIC
    state.cpp
    task_state_machine.cpp
)

target_include_directories(moltcat_model
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_model
    PUBLIC
        moltcat_utils
)
