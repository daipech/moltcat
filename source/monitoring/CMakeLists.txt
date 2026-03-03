# Monitoring library
add_library(moltcat_monitoring STATIC
    agent_health_monitor.cpp
    unified_monitor.cpp
)

target_include_directories(moltcat_monitoring
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_monitoring
    PUBLIC
        moltcat_model
        moltcat_utils
)
