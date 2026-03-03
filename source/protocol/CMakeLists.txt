# Protocol interface library (header-only)
add_library(moltcat_protocol INTERFACE)

target_include_directories(moltcat_protocol
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_protocol
    INTERFACE
        moltcat_model
        moltcat_utils
)
