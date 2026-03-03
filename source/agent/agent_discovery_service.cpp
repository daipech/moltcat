#include "agent/agent_discovery_service.hpp"
#include "messaging/message_bus.hpp"
#include <glaze/json.hpp>
#include <chrono>

namespace moltcat::agent {

// ================================
// Constructor
// ================================

AgentDiscoveryService::AgentDiscoveryService(
    std::shared_ptr<TeamRegistry> registry,
    std::shared_ptr<spdlog::logger> logger
)
    : registry_(std::move(registry))
    , logger_(logger ? logger : spdlog::default_logger())
{
    logger_->info("AgentDiscoveryService created");
}

// ================================
// IMessageHandler Interface Implementation
// ================================

auto AgentDiscoveryService::handle_message(const messaging::Message& msg)
    -> std::optional<messaging::Message>
{
    if (!running_) {
        logger_->warn("Service not running, ignoring message: {}", msg.header.message_id);
        return std::nullopt;
    }

    switch (msg.header.type) {
        case messaging::MessageType::AGENT_REGISTER:
            return handle_register(msg);

        case messaging::MessageType::AGENT_UNREGISTER:
            return handle_unregister(msg);

        case messaging::MessageType::TEAM_DISCOVER_MEMBERS:
            return handle_discover_members(msg);

        default:
            // Unhandled message type
            return std::nullopt;
    }
}

auto AgentDiscoveryService::can_handle(messaging::MessageType type) const -> bool {
    return type == messaging::MessageType::AGENT_REGISTER ||
           type == messaging::MessageType::AGENT_UNREGISTER ||
           type == messaging::MessageType::TEAM_DISCOVER_MEMBERS;
}

// ================================
// Lifecycle Management
// ================================

auto AgentDiscoveryService::start() -> void {
    if (running_) {
        logger_->warn("AgentDiscoveryService already running");
        return;
    }

    running_ = true;
    logger_->info("AgentDiscoveryService started");
}

auto AgentDiscoveryService::stop() -> void {
    if (!running_) {
        logger_->warn("AgentDiscoveryService not running");
        return;
    }

    running_ = false;
    logger_->info("AgentDiscoveryService stopped");
}

// ================================
// Message Handling Methods
// ================================

auto AgentDiscoveryService::handle_register(const messaging::Message& msg)
    -> std::optional<messaging::Message>
{
    logger_->info("Processing AGENT_REGISTER from {}", msg.header.source);

    // Parse registration request
    glz::json_t request_json;
    auto err = glz::read_json<glz::json_t>(request_json, msg.body.content);

    if (err) {
        logger_->error("Failed to parse AGENT_REGISTER: {}", err);
        return create_error_response(msg.header.message_id, service_queue_name_,
            "Invalid JSON format");
    }

    auto request = messaging::AgentRegisterRequest::deserialize(request_json);
    if (!request) {
        logger_->error("Invalid AGENT_REGISTER request format");
        return create_error_response(msg.header.message_id, service_queue_name_,
            "Invalid request format");
    }

    // Validate required fields
    if (request->agent_id.empty() || request->team_id.empty() ||
        request->a2a_address.empty()) {
        logger_->error("Missing required fields in AGENT_REGISTER");
        return create_error_response(msg.header.message_id, service_queue_name_,
            "Missing required fields: agent_id, team_id, a2a_address");
    }

    // Build AgentInfo
    messaging::AgentInfo info;
    info.agent_id = request->agent_id;
    info.team_id = request->team_id;
    info.a2a_address = request->a2a_address;
    info.capabilities = request->capabilities;
    info.status = messaging::AgentStatus::ONLINE;
    info.last_heartbeat = messaging::Message::current_timestamp();
    info.metadata = request->metadata;

    // Register to TeamRegistry
    registry_->register_agent(info);

    // Broadcast member joined event
    broadcast_member_joined(info);

    // Return success response
    logger_->info("Agent {} registered to team {} at {}",
        info.agent_id, info.team_id, info.a2a_address);

    return create_success_response(msg.header.message_id, service_queue_name_);
}

auto AgentDiscoveryService::handle_unregister(const messaging::Message& msg)
    -> std::optional<messaging::Message>
{
    logger_->info("Processing AGENT_UNREGISTER from {}", msg.header.source);

    // Parse unregistration request
    glz::json_t request_json;
    auto err = glz::read_json<glz::json_t>(request_json, msg.body.content);

    if (err) {
        logger_->error("Failed to parse AGENT_UNREGISTER: {}", err);
        return create_error_response(msg.header.message_id, service_queue_name_,
            "Invalid JSON format");
    }

    std::string agent_id;
    if (request_json.contains("agent_id")) {
        agent_id = request_json["agent_id"].get<std::string>();
    } else {
        // Use message source as agent_id
        agent_id = msg.header.source;
    }

    if (agent_id.empty()) {
        logger_->error("Missing agent_id in AGENT_UNREGISTER");
        return create_error_response(msg.header.message_id, service_queue_name_,
            "Missing agent_id");
    }

    // Query Agent's Team
    auto team_id = registry_->get_agent_team(agent_id);

    // Remove from TeamRegistry
    registry_->unregister_agent(agent_id);

    // Broadcast member left event
    if (team_id) {
        broadcast_member_left(agent_id, *team_id);
        logger_->info("Agent {} unregistered from team {}", agent_id, *team_id);
    } else {
        logger_->warn("Agent {} was not registered in any team", agent_id);
    }

    return create_success_response(msg.header.message_id, service_queue_name_);
}

auto AgentDiscoveryService::handle_discover_members(const messaging::Message& msg)
    -> std::optional<messaging::Message>
{
    logger_->info("Processing TEAM_DISCOVER_MEMBERS from {}", msg.header.source);

    // Parse query request
    glz::json_t request_json;
    auto err = glz::read_json<glz::json_t>(request_json, msg.body.content);

    if (err) {
        logger_->error("Failed to parse TEAM_DISCOVER_MEMBERS: {}", err);
        return create_error_response(msg.header.message_id, service_queue_name_,
            "Invalid JSON format");
    }

    std::string team_id;
    if (request_json.contains("team_id")) {
        team_id = request_json["team_id"].get<std::string>();
    } else {
        logger_->error("Missing team_id in TEAM_DISCOVER_MEMBERS");
        return create_error_response(msg.header.message_id, service_queue_name_,
            "Missing team_id");
    }

    // Query team members
    auto members = registry_->get_team_members(team_id);

    // Construct response
    messaging::TeamMembersResponse response;
    response.team_id = team_id;
    response.members = members;

    // Serialize response
    std::string response_json;
    glz::write_json(response.serialize(), response_json);

    // Construct response message
    messaging::Message response_msg = messaging::Message::create(
        messaging::MessageType::TEAM_MEMBERS_RESPONSE,
        service_queue_name_
    );
    response_msg.header.destination = msg.header.source;
    response_msg.header.correlation_id = msg.header.message_id;
    response_msg.body.content = response_json;

    logger_->info("Returning {} members for team {}", members.size(), team_id);

    return response_msg;
}

auto AgentDiscoveryService::broadcast_member_joined(const messaging::AgentInfo& info)
    -> void
{
    // Construct broadcast message
    messaging::Message event = messaging::Message::create(
        messaging::MessageType::TEAM_MEMBER_JOINED,
        service_queue_name_
    );

    // Serialize AgentInfo
    std::string info_json;
    glz::write_json(info, info_json);  // Use Glaze to serialize
    event.body.content = info_json;

    // Broadcast via MessageBusClient (if MessageBusClient reference available)
    if (message_bus_client_) {
        // Broadcast to "team_updates" topic or service
        size_t sent_count = message_bus_client_->broadcast("team_updates", event);
        logger_->debug("Broadcasted TEAM_MEMBER_JOINED for agent {} in team {} to {} recipients",
            info.agent_id, info.team_id, sent_count);
    } else {
        logger_->warn("Cannot broadcast TEAM_MEMBER_JOINED: no MessageBusClient reference");
    }
}

auto AgentDiscoveryService::broadcast_member_left(
    const std::string& agent_id,
    const std::string& team_id
) -> void {
    // Construct broadcast message
    messaging::Message event = messaging::Message::create(
        messaging::MessageType::TEAM_MEMBER_LEFT,
        service_queue_name_
    );

    // Serialize event data
    glz::json_t event_json{
        {"agent_id", agent_id},
        {"team_id", team_id}
    };

    std::string event_json_str;
    glz::write_json(event_json, event_json_str);
    event.body.content = event_json_str;

    // Broadcast via MessageBusClient
    if (message_bus_client_) {
        // Broadcast to "team_updates" topic or service
        size_t sent_count = message_bus_client_->broadcast("team_updates", event);
        logger_->debug("Broadcasted TEAM_MEMBER_LEFT for agent {} in team {} to {} recipients",
            agent_id, team_id, sent_count);
    } else {
        logger_->warn("Cannot broadcast TEAM_MEMBER_LEFT: no MessageBusClient reference");
    }
}

// ================================
// Helper Methods
// ================================

auto AgentDiscoveryService::create_success_response(
    const std::string& original_message_id,
    const std::string& source
) -> messaging::Message {
    messaging::Message response = messaging::Message::create(
        messaging::MessageType::TEAM_MEMBERS_RESPONSE,
        source
    );
    response.header.correlation_id = original_message_id;

    glz::json_t response_json{
        {"status", "success"},
        {"message", "Operation completed successfully"}
    };

    std::string response_json_str;
    glz::write_json(response_json, response_json_str);
    response.body.content = response_json_str;

    return response;
}

auto AgentDiscoveryService::create_error_response(
    const std::string& original_message_id,
    const std::string& source,
    const std::string& error_message
) -> messaging::Message {
    messaging::Message response = messaging::Message::create(
        messaging::MessageType::SYSTEM_ERROR,
        source
    );
    response.header.correlation_id = original_message_id;

    glz::json_t response_json{
        {"status", "error"},
        {"message", error_message}
    };

    std::string response_json_str;
    glz::write_json(response_json, response_json_str);
    response.body.content = response_json_str;

    return response;
}

} // namespace moltcat::agent
