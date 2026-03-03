# Network client library
add_library(moltcat_network STATIC
    llm_client_factory.cpp
    llm_request.cpp
    llm_response.cpp
    mock_llm_client.cpp
)

target_include_directories(moltcat_network
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_network
    PUBLIC
        moltcat_model
        moltcat_utils
)
