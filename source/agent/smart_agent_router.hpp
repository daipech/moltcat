#pragma once

#include "messaging/message.hpp"
#include "messaging/message_bus.hpp"
#include "team_registry.hpp"
#include <string>
#include <memory>
#include <functional>
#include <future>
#include <optional>
#include <spdlog/spdlog.h>

namespace moltcat::agent {

/**
 * SmartAgentRouter - Intelligent Agent router
 *
 * Responsibilities:
 * 1. Automatically select optimal communication path based on target Agent's Team membership
 * 2. Intra-Team communication: use A2A (low latency < 3ms)
 * 3. Cross-Team communication: use MessageBus (decoupled, 10-50ms)
 * 4. Provide intra-Team broadcast functionality (A2A PUB-SUB)
 *
 * Routing decision logic:
 * ```
 * if (target_team_id == my_team_id) {
 *     return send_via_a2a(target, msg);  // Same Team, low latency
 * } else {
 *     return send_via_message_bus(target, msg);  // Cross Team, decoupled
 * }
 * ```
 *
 * Performance comparison:
 * - Intra-Team A2A: < 3ms, >1000 msg/s
 * - Cross-Team MessageBus: 10-50ms, <100 msg/s
 */
class SmartAgentRouter {
public:
    // ================================
    // Type Definitions
    // ================================

    /**
     * Response type (using std::future for asynchronous implementation)
     */
    using ResponseFuture = std::future<std::optional<messaging::Message>>;

    /**
     * Message send callback
     */
    using MessageCallback = std::function<void(const std::optional<messaging::Message>&)>;

    // ================================
    // Constructors
    // ================================

    /**
     * Constructor
     *
     * @param my_agent_id This Agent ID
     * @param my_team_id This Team ID
     * @param message_bus_client MessageBusClient reference (for cross-Team communication)
     * @param registry TeamRegistry reference (for querying Agent information)
     * @param logger Logger (optional)
     */
    SmartAgentRouter(
        std::string my_agent_id,
        std::string my_team_id,
        messaging::MessageBusClient* message_bus_client,
        TeamRegistry* registry,
        std::shared_ptr<spdlog::logger> logger = nullptr
    );

    ~SmartAgentRouter() = default;

    // Disable copy and move
    SmartAgentRouter(const SmartAgentRouter&) = delete;
    SmartAgentRouter& operator=(const SmartAgentRouter&) = delete;
    SmartAgentRouter(SmartAgentRouter&&) = delete;
    SmartAgentRouter& operator=(SmartAgentRouter&&) = delete;

    // ================================
    // Core Routing Functionality
    // ================================

    /**
     * Send message (auto-select route)
     *
     * Intelligent decision:
     * 1. Query target Agent information (from TeamRegistry)
     * 2. Determine if in same Team
     * 3. Same Team: use A2A (low latency)
     * 4. Cross Team: use MessageBus (decoupled)
     *
     * @param target_agent_id Target Agent ID
     * @param msg Message
     * @return Asynchronous response (std::future)
     *
     * @note Thread-safe
     */
    auto send_message(
        const std::string& target_agent_id,
        const messaging::Message& msg
    ) -> ResponseFuture;

    /**
     * Send message (callback version)
     *
     * @param target_agent_id Target Agent ID
     * @param msg Message
     * @param callback Response callback
     *
     * @note Thread-safe
     */
    auto send_message_async(
        const std::string& target_agent_id,
        const messaging::Message& msg,
        MessageCallback callback
    ) -> void;

    /**
     * Broadcast message to all Team members
     *
     * Broadcast message to all Team members via A2A PUB-SUB
     *
     * @param msg Message (A2A protocol format)
     *
     * @note Only broadcast to same Team members
     * @note Use A2A protocol (low latency < 3ms)
     * @note Thread-safe
     */
    auto broadcast_to_team(const protocol::a2a::Message& msg) -> void;

    /**
     * Send RPC request
     *
     * @param target_agent_id Target Agent ID
     * @param method Method name
     * @param params Parameters (JSON format)
     * @param timeout_ms Timeout (milliseconds)
     * @return Asynchronous response
     *
     * @note Auto-select route (A2A or MessageBus)
     * @note Thread-safe
     */
    auto send_rpc(
        const std::string& target_agent_id,
        const std::string& method,
        const glz::json_t& params,
        uint64_t timeout_ms = 5000
    ) -> ResponseFuture;

    // ================================
    // Routing Strategy Query
    // ================================

    /**
     * Check if target Agent is in same Team
     *
     * @param target_agent_id Target Agent ID
     * @return true indicates same Team
     *
     * @note Thread-safe (shared lock)
     */
    [[nodiscard]] auto is_same_team(const std::string& target_agent_id) const -> bool;

    /**
     * Get target Agent's Team ID
     *
     * @param target_agent_id Target Agent ID
     * @return Team ID (returns nullopt if not found)
     *
     * @note Thread-safe (shared lock)
     */
    [[nodiscard]] auto get_agent_team(const std::string& target_agent_id) const
        -> std::optional<std::string>;

    /**
     * Predict routing strategy
     *
     * @param target_agent_id Target Agent ID
     * @return "A2A" or "MessageBus"
     *
     * @note For debugging and monitoring
     * @note Thread-safe (shared lock)
     */
    [[nodiscard]] auto predict_route(const std::string& target_agent_id) const
        -> std::string;

    // ================================
    // Statistics
    // ================================

    /**
     * Routing statistics
     */
    struct RouteStats {
        uint64_t a2a_routes = 0;       // A2A route count
        uint64_t messagebus_routes = 0;  // MessageBus route count
        uint64_t failed_routes = 0;      // Failed route count
        uint64_t total_routes = 0;       // Total route count
    };

    /**
     * Get routing statistics
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto get_stats() const -> RouteStats;

    /**
     * Reset statistics
     *
     * @note Thread-safe
     */
    auto reset_stats() -> void;

    // ================================
    // Accessors
    // ================================

    [[nodiscard]] auto get_my_agent_id() const noexcept -> const std::string& {
        return my_agent_id_;
    }

    [[nodiscard]] auto get_my_team_id() const noexcept -> const std::string& {
        return my_team_id_;
    }

private:
    // ================================
    // Internal Routing Methods
    // ================================

    /**
     * Send via A2A (intra-Team, low latency)
     *
     * @param target Target Agent information
     * @param msg Message
     * @return Asynchronous response
     *
     * @note Direct send via ZeroMQ A2A
     * @note Latency < 3ms
     */
    auto send_via_a2a(
        const messaging::AgentInfo& target,
        const messaging::Message& msg
    ) -> ResponseFuture;

    /**
     * Send via MessageBus (cross-Team, decoupled)
     *
     * @param target_agent_id Target Agent ID
     * @param msg Message
     * @return Asynchronous response
     *
     * @note Route via MessageBus
     * @note Latency 10-50ms
     */
    auto send_via_message_bus(
        const std::string& target_agent_id,
        const messaging::Message& msg
    ) -> ResponseFuture;

    /**
     * Execute A2A communication (simulated implementation)
     *
     * TODO: Integrate actual A2A Client
     *
     * @param a2a_address Target A2A address
     * @param msg Message
     * @return Asynchronous response
     */
    auto execute_a2a_communication(
        const std::string& a2a_address,
        const messaging::Message& msg
    ) -> ResponseFuture;

    /**
     * Execute MessageBus communication
     *
     * @param target_agent_id Target Agent ID
     * @param msg Message
     * @return Asynchronous response
     */
    auto execute_messagebus_communication(
        const std::string& target_agent_id,
        const messaging::Message& msg
    ) -> ResponseFuture;

    // ================================
    // Fields
    // ================================

    std::string my_agent_id_;                           // This Agent ID
    std::string my_team_id_;                            // This Team ID
    std::string my_a2a_address_;                        // This Agent A2A address

    messaging::MessageBusClient* message_bus_client_;   // MessageBusClient reference (no ownership)
    TeamRegistry* registry_;                            // TeamRegistry reference (no ownership)

    std::shared_ptr<spdlog::logger> logger_;            // Logger

    // Statistics (thread-safe)
    mutable std::mutex stats_mutex_;
    RouteStats stats_;

    // TODO: A2A Client (to be implemented)
    // std::unique_ptr<A2AClient> a2a_client_;
};

} // namespace moltcat::agent
