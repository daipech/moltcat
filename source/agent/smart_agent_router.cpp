#include "agent/smart_agent_router.hpp"
#include "protocol/a2a_types.hpp"
#include <thread>
#include <chrono>

namespace moltcat::agent {

// ================================
// Constructor
// ================================

SmartAgentRouter::SmartAgentRouter(
    std::string my_agent_id,
    std::string my_team_id,
    messaging::MessageBusClient* message_bus_client,
    TeamRegistry* registry,
    std::shared_ptr<spdlog::logger> logger
)
    : my_agent_id_(std::move(my_agent_id))
    , my_team_id_(std::move(my_team_id))
    , message_bus_client_(message_bus_client)
    , registry_(registry)
    , logger_(logger ? logger : spdlog::default_logger())
{
    if (!message_bus_client_) {
        logger_->error("SmartAgentRouter: MessageBusClient cannot be null");
        throw std::invalid_argument("message_bus_client cannot be null");
    }

    if (!registry_) {
        logger_->error("SmartAgentRouter: TeamRegistry cannot be null");
        throw std::invalid_argument("registry cannot be null");
    }

    // Generate A2A address (based on hash)
    size_t hash = std::hash<std::string>{}(my_agent_id_);
    uint16_t port = 5570 + (hash % 1000);  // Port range: 5570-6569
    my_a2a_address_ = std::format("tcp://*:{}", port);

    logger_->info("SmartAgentRouter initialized: agent={}, team={}, a2a_address={}",
                  my_agent_id_, my_team_id_, my_a2a_address_);

    // TODO: Initialize A2A Client
    // a2a_client_ = std::make_unique<A2AClient>(context_);
    // a2a_client_->bind(my_a2a_address_);
}

// ================================
// Core Routing Functionality
// ================================

auto SmartAgentRouter::send_message(
    const std::string& target_agent_id,
    const messaging::Message& msg
) -> ResponseFuture {
    // Update statistics
    {
        std::lock_guard lock(stats_mutex_);
        stats_.total_routes++;
    }

    // 1. Query target Agent information
    auto target_info = registry_->get_agent_info(target_agent_id);

    if (!target_info) {
        logger_->error("Target Agent does not exist: {}", target_agent_id);

        std::lock_guard lock(stats_mutex_);
        stats_.failed_routes++;

        // Return a completed future (containing error)
        std::promise<std::optional<messaging::Message>> promise;
        promise.set_value(std::nullopt);
        return promise.get_future();
    }

    // 2. Determine if in same Team
    if (target_info->team_id == my_team_id_) {
        // Intra-Team communication: use A2A (low latency)
        logger_->debug("Routing to {} (same Team) via A2A: {}",
                       target_agent_id, target_info->a2a_address);

        std::lock_guard lock(stats_mutex_);
        stats_.a2a_routes++;

        return send_via_a2a(*target_info, msg);

    } else {
        // Cross-Team communication: use MessageBus (decoupled)
        logger_->debug("Routing to {} (cross Team) via MessageBus: {} -> {}",
                       target_agent_id, my_team_id_, target_info->team_id);

        std::lock_guard lock(stats_mutex_);
        stats_.messagebus_routes++;

        return send_via_message_bus(target_agent_id, msg);
    }
}

auto SmartAgentRouter::send_message_async(
    const std::string& target_agent_id,
    const messaging::Message& msg,
    MessageCallback callback
) -> void {
    // Execute routing in background thread
    std::thread([this, target_agent_id, msg, callback]() {
        auto future = send_message(target_agent_id, msg);
        auto response = future.get();  // Block waiting for response
        callback(response);
    }).detach();
}

auto SmartAgentRouter::broadcast_to_team(const protocol::a2a::Message& msg) -> void {
    logger_->debug("Team {} broadcasting message to all members (A2A PUB-SUB: {})",
                   my_team_id_, my_team_id_ + ".updates");

    // TODO: Broadcast via A2A Publisher
    //
    // Example code:
    // if (a2a_client_) {
    //     a2a_client_->publish(my_team_id_ + ".updates", msg);
    // }

    // Temporary implementation: send to all known members
    auto members = registry_->get_team_members(my_team_id_);
    logger_->debug("Broadcasting to {} members", members.size());

    for (const auto& member : members) {
        if (member.agent_id != my_agent_id_) {  // Don't send to self
            try {
                messaging::Message wrapper_msg = messaging::Message::create(
                    messaging::MessageType::AGENT_MESSAGE,
                    my_agent_id_
                );
                wrapper_msg.header.destination = member.agent_id;

                // Serialize A2A Message
                std::string msg_json;
                glz::write_json(msg, msg_json);
                wrapper_msg.body.content = msg_json;

                send_via_a2a(member, wrapper_msg);

            } catch (const std::exception& e) {
                logger_->error("Broadcast to {} failed: {}", member.agent_id, e.what());
            }
        }
    }
}

auto SmartAgentRouter::send_rpc(
    const std::string& target_agent_id,
    const std::string& method,
    const glz::json_t& params,
    uint64_t timeout_ms
) -> ResponseFuture {
    // Construct RPC request message
    messaging::Message rpc_msg = messaging::Message::create(
        messaging::MessageType::AGENT_RPC,
        my_agent_id_
    );
    rpc_msg.header.destination = target_agent_id;

    // Serialize RPC request
    messaging::AgentRpcRequest rpc_request;
    rpc_request.target_agent_id = target_agent_id;
    rpc_request.method = method;
    rpc_request.params = params;
    rpc_request.timeout_ms = timeout_ms;

    std::string request_json;
    glz::write_json(rpc_request.serialize(), request_json);
    rpc_msg.body.content = request_json;

    // Send using intelligent routing
    return send_message(target_agent_id, rpc_msg);
}

// ================================
// Routing Strategy Query
// ================================

auto SmartAgentRouter::is_same_team(const std::string& target_agent_id) const -> bool {
    auto target_team_id = get_agent_team(target_agent_id);
    return target_team_id && *target_team_id == my_team_id_;
}

auto SmartAgentRouter::get_agent_team(const std::string& target_agent_id) const
    -> std::optional<std::string>
{
    if (!registry_) {
        return std::nullopt;
    }

    auto agent_info = registry_->get_agent_info(target_agent_id);
    if (agent_info) {
        return agent_info->team_id;
    }

    return std::nullopt;
}

auto SmartAgentRouter::predict_route(const std::string& target_agent_id) const
    -> std::string
{
    if (is_same_team(target_agent_id)) {
        return "A2A";
    } else {
        return "MessageBus";
    }
}

// ================================
// Statistics
// ================================

auto SmartAgentRouter::get_stats() const -> RouteStats {
    std::lock_guard lock(stats_mutex_);
    return stats_;
}

auto SmartAgentRouter::reset_stats() -> void {
    std::lock_guard lock(stats_mutex_);
    stats_ = RouteStats{};
    logger_->info("SmartAgentRouter statistics reset");
}

// ================================
// Internal Routing Methods
// ================================

auto SmartAgentRouter::send_via_a2a(
    const messaging::AgentInfo& target,
    const messaging::Message& msg
) -> ResponseFuture {
    logger_->debug("Sending via A2A to: {} (address: {})",
                   target.agent_id, target.a2a_address);

    return execute_a2a_communication(target.a2a_address, msg);
}

auto SmartAgentRouter::send_via_message_bus(
    const std::string& target_agent_id,
    const messaging::Message& msg
) -> ResponseFuture {
    logger_->debug("Sending via MessageBus to: {}", target_agent_id);

    return execute_messagebus_communication(target_agent_id, msg);
}

auto SmartAgentRouter::execute_a2a_communication(
    const std::string& a2a_address,
    const messaging::Message& msg
) -> ResponseFuture {
    // TODO: Integrate actual A2A Client
    //
    // Current implementation: simulated asynchronous response
    //
    // Actual implementation should be:
    // return a2a_client_->send_async(a2a_address, msg);

    std::promise<std::optional<messaging::Message>> promise;

    // Simulate A2A communication in background thread
    std::thread([this, a2a_address, msg, promise = std::move(promise)]() mutable {
        try {
            // Simulate network latency (< 3ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));

            // Construct response
            messaging::Message response = messaging::Message::create(
                messaging::MessageType::TEAM_MEMBER_JOINED,
                msg.header.destination  // Responder as sender
            );
            response.header.source = msg.header.destination;
            response.header.destination = msg.header.source;
            response.header.correlation_id = msg.header.message_id;
            response.body.content = R"({"status": "ok", "via": "A2A"})";

            logger_->debug("A2A response received: {}", a2a_address);
            promise.set_value(response);

        } catch (const std::exception& e) {
            logger_->error("A2A communication failed: {}", e.what());
            promise.set_value(std::nullopt);
        }
    }).detach();

    return promise.get_future();
}

auto SmartAgentRouter::execute_messagebus_communication(
    const std::string& target_agent_id,
    const messaging::Message& msg
) -> ResponseFuture {
    // TODO: Integrate MessageBusClient asynchronous sending functionality
    //
    // Current implementation: simulated asynchronous response
    //
    // Actual implementation should be:
    // return message_bus_client_->send_async(msg);

    std::promise<std::optional<messaging::Message>> promise;

    // Simulate MessageBus communication in background thread
    std::thread([this, target_agent_id, msg, promise = std::move(promise)]() mutable {
        try {
            // Send message via MessageBusClient
            bool sent = message_bus_client_->send(msg);

            if (!sent) {
                logger_->error("MessageBusClient send failed: {}", target_agent_id);
                promise.set_value(std::nullopt);
                return;
            }

            // Simulate network latency (10-50ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            // Construct response
            messaging::Message response = messaging::Message::create(
                messaging::MessageType::TEAM_MEMBER_JOINED,
                target_agent_id
            );
            response.header.source = target_agent_id;
            response.header.destination = msg.header.source;
            response.header.correlation_id = msg.header.message_id;
            response.body.content = R"({"status": "ok", "via": "MessageBus"})";

            logger_->debug("MessageBusClient response received: {}", target_agent_id);
            promise.set_value(response);

        } catch (const std::exception& e) {
            logger_->error("MessageBusClient communication failed: {}", e.what());
            promise.set_value(std::nullopt);
        }
    }).detach();

    return promise.get_future();
}

} // namespace moltcat::agent
