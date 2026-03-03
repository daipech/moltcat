# Gateway library
add_library(moltcat_gateway STATIC
    http_gateway.cpp
    ws_gateway.cpp
    task_manager.cpp
    auth/auth_manager.cpp
    auth/permission_checker.cpp
    auth/signature_verifier.cpp
    auth/token_manager.cpp
)

target_include_directories(moltcat_gateway
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_gateway
    PUBLIC
        moltcat_messaging
        moltcat_utils
        moltcat_model
        moltcat_agent
    PRIVATE
        uwebsockets
        httplib
)
