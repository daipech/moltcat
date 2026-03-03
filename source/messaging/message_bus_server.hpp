#pragma once

#include "message.hpp"
#include "service_registry.hpp"
#include "message_router.hpp"
#include <zmq.hpp>
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <spdlog/spdlog.h>

namespace moltcat::messaging {

/**
 * @brief MessageBus server status
 */
struct ServerStatus {
    bool running = false;
    size_t connected_clients = 0;
    size_t registered_services = 0;
    uint64_t messages_routed = 0;
    uint64_t routing_errors = 0;
};

/**
 * @brief Single-node MessageBus server
 *
 * Responsibilities:
 * 1. Listen for client connections (ROUTER socket)
 * 2. Maintain service registry
 * 3. Route messages to target services
 * 4. Manage subscription relationships (PUB socket for broadcast)
 *
 * Architecture notes:
 * - Uses ZeroMQ ROUTER socket to handle client connections (DEALER-ROUTER pattern)
 * - Uses ZeroMQ PUB socket for broadcast (SUB-PUB pattern)
 * - Message loop runs in a separate thread
 */
class MessageBusServer {
public:
    /**
     * @brief Server configuration
     */
    struct Config {
        // Listen endpoint
        // IPC: "ipc:///tmp/moltcat_messagebus.sock"
        // TCP: "tcp://*:5560" or "tcp://10.0.0.1:5560"
        std::string endpoint = "ipc:///tmp/moltcat_messagebus.sock";

        // Broadcast endpoint (optional)
        std::string pub_endpoint = "ipc:///tmp/moltcat_messagebus_pub.sock";

        // Number of worker threads (default: 1, single ZeroMQ thread is sufficient)
        size_t worker_threads = 1;

        // Number of I/O threads (ZeroMQ internal)
        // Recommendation: Adjust based on CPU cores and network load
        // - Single-core or light load: 1-2
        // - Multi-core or heavy load: 4
        // - Note: Increasing I/O threads only improves network concurrency, does not solve message processing bottleneck
        int io_threads = 4;

        // Load balancing strategy
        LoadBalanceStrategy lb_strategy = LoadBalanceStrategy::ROUND_ROBIN;

        // Receive timeout (milliseconds, -1 means blocking)
        int recv_timeout_ms = -1;

        // Send timeout (milliseconds, -1 means blocking)
        int send_timeout_ms = -1;

        // Log level
        spdlog::level::level_enum log_level = spdlog::level::info;
    };

    explicit MessageBusServer(const Config& config);
    ~MessageBusServer();

    // Disable copy and move
    MessageBusServer(const MessageBusServer&) = delete;
    MessageBusServer& operator=(const MessageBusServer&) = delete;
    MessageBusServer(MessageBusServer&&) = delete;
    MessageBusServer& operator=(MessageBusServer&&) = delete;

    /**
     * @brief Start server (blocking call)
     *
     * @return true indicates successful startup
     */
    auto start() -> bool;

    /**
     * @brief Stop server
     */
    auto stop() -> void;

    /**
     * @brief Get server status
     */
    auto get_status() const -> ServerStatus;

    /**
     * @brief Get service registry (for debugging and management)
     */
    auto get_registry() -> ServiceRegistry& { return *registry_; }
    auto get_registry() const -> const ServiceRegistry& { return *registry_; }

    /**
     * @brief Set load balancing strategy
     */
    auto set_load_balance_strategy(LoadBalanceStrategy strategy) -> void;

private:
    Config config_;

    // ZeroMQ context
    zmq::context_t context_;

    // ROUTER socket (for communicating with clients)
    zmq::socket_t router_socket_;

    // PUB socket (for broadcast)
    zmq::socket_t pub_socket_;

    // Service registry
    std::unique_ptr<ServiceRegistry> registry_;

    // Message router
    std::unique_ptr<MessageRouter> router_;

    // Running state
    std::atomic<bool> running_{false};

    // Message loop thread
    std::thread message_loop_thread_;

    // Statistics
    std::atomic<uint64_t> messages_routed_{0};
    std::atomic<uint64_t> routing_errors_{0};
    std::atomic<size_t> connected_clients_{0};

    // Logger
    std::shared_ptr<spdlog::logger> logger_;

    /**
     * @brief Internal implementation methods
     */

    // Message loop
    auto message_loop() -> void;

    // Handle received message
    auto handle_message(
        const std::string& sender_id,
        const Message& msg
    ) -> void;

    // Handle service registration
    auto handle_service_register(
        const std::string& queue_id,
        const Message& msg
    ) -> void;

    // Handle service unregistration
    auto handle_service_unregister(
        const std::string& queue_id,
        const Message& msg
    ) -> void;

    // Handle topic subscription
    auto handle_subscribe(
        const std::string& queue_id,
        const Message& msg
    ) -> void;

    // Handle topic unsubscription
    auto handle_unsubscribe(
        const std::string& queue_id,
        const Message& msg
    ) -> void;

    // Send message to specified queue (via ROUTER socket)
    auto send_to_queue(
        const std::string& queue_id,
        const Message& msg
    ) -> bool;

    // Broadcast message (via PUB socket)
    auto broadcast_message(const Message& msg) -> void;

    // Send response message
    auto send_response(
        const std::string& queue_id,
        const Message& request,
        const std::string& response_content
    ) -> void;
};

} // namespace moltcat::messaging
