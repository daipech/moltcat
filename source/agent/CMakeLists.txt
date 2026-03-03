# Agent core library
add_library(moltcat_agent STATIC
    agent.cpp
    agent_discovery_service.cpp
    agent_pool.cpp
    agent_team.cpp
    agent_template.cpp
    agent_template_builder.cpp
    agent_template_registry.cpp
    smart_agent_router.cpp
    task_classifier.cpp
    task_router.cpp
    team_registry.cpp
)

target_include_directories(moltcat_agent
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_agent
    PUBLIC
        moltcat_model
        moltcat_memory
        moltcat_messaging
        moltcat_protocol
        moltcat_provider
        moltcat_utils
)
