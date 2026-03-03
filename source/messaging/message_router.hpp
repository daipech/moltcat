#pragma once

#include "message.hpp"
#include "service_registry.hpp"
#include "load_balancer.hpp"
#include <zmq.hpp>
#include <string>
#include <memory>
#include <spdlog/spdlog.h>

namespace moltcat::messaging {

/**
 * @brief Routing result
 */
struct RoutingResult {
    bool success = false;
    size_t recipients_count = 0;
    std::string error_message;
};

/**
 * @brief Message router
 *
 * Responsibilities:
 * 1. Route messages based on message type and destination address
 * 2. Support point-to-point routing, broadcast, publish/subscribe
 * 3. Use load balancing strategy to select target queue
 *
 * Design notes:
 * - Does not directly operate ZeroMQ sockets, caller passes send function
 * - Send messages via callback function (decouple routing logic from transport logic)
 */
class MessageRouter {
public:
    /**
     * @brief Send function type
     *
     * @param recipient_id Recipient ID (queue ID)
     * @param msg Message
     * @return true indicates successful send
     */
    using SendFunction = std::function<bool(
        const std::string& recipient_id,
        const Message& msg
    )>;

    MessageRouter(
        ServiceRegistry& registry,
        LoadBalanceStrategy lb_strategy = LoadBalanceStrategy::ROUND_ROBIN
    );
    ~MessageRouter() = default;

    // Disable copy and move
    MessageRouter(const MessageRouter&) = delete;
    MessageRouter& operator=(const MessageRouter&) = delete;
    MessageRouter(MessageRouter&&) = delete;
    MessageRouter& operator=(MessageRouter&&) = delete;

    /**
     * @brief Route message (automatically select routing method based on message type)
     *
     * @param msg Message
     * @param sender_id Sender ID
     * @param send_func Send function
     * @return Routing result
     */
    auto route(
        const Message& msg,
        const std::string& sender_id,
        SendFunction send_func
    ) -> RoutingResult;

    /**
     * @brief Point-to-point routing (send to one instance of specified service)
     *
     * @param msg Message
     * @param sender_id Sender ID
     * @param send_func Send function
     * @return Routing result
     */
    auto route_point_to_point(
        const Message& msg,
        const std::string& sender_id,
        SendFunction send_func
    ) -> RoutingResult;

    /**
     * @brief Broadcast (send to all instances of specified service)
     *
     * @param msg Message
     * @param sender_id Sender ID
     * @param send_func Send function
     * @return Routing result
     */
    auto broadcast(
        const Message& msg,
        const std::string& sender_id,
        SendFunction send_func
    ) -> RoutingResult;

    /**
     * @brief Publish to topic (send to all subscribers)
     *
     * @param msg Message
     * @param topic Topic
     * @param sender_id Sender ID
     * @param send_func Send function
     * @return Routing result
     */
    auto publish_to_topic(
        const Message& msg,
        const std::string& topic,
        const std::string& sender_id,
        SendFunction send_func
    ) -> RoutingResult;

    /**
     * @brief Set load balancing strategy
     */
    auto set_load_balance_strategy(LoadBalanceStrategy strategy) -> void;

private:
    ServiceRegistry& registry_;
    std::unique_ptr<LoadBalancer> load_balancer_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace moltcat::messaging
