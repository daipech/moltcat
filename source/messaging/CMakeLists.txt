# Messaging library
add_library(moltcat_messaging STATIC
    message_bus_client.cpp
    message_bus_server.cpp
    message_router.cpp
    load_balancer.cpp
    service_registry.cpp
)

target_include_directories(moltcat_messaging
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_messaging
    PUBLIC
        moltcat_utils
        moltcat_model
        moltcat_network
    PRIVATE
        libzmq
        libzmq-static
        cppzmq
)
