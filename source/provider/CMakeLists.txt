# Provider management library
add_library(moltcat_provider STATIC
    provider.cpp
    provider_manager.cpp
)

target_include_directories(moltcat_provider
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_provider
    PUBLIC
        moltcat_model
        moltcat_network
        moltcat_utils
        glaze::glaze
)
