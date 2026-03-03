#pragma once

#include "message.hpp"
#include <zmq.hpp>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <optional>
#include <spdlog/spdlog.h>

namespace moltcat::messaging {

/**
 * MessageBus client
 *
 * Responsibilities:
 * 1. Connect to MessageBus server (DEALER socket)
 * 2. Register service to server
 * 3. Send and receive messages
 * 4. Subscribe to topics (SUB socket)
 * 5. Handle message loop
 *
 * Architecture notes:
 * - Uses ZeroMQ DEALER socket to connect to server's ROUTER
 * - Uses ZeroMQ SUB socket to subscribe to broadcasts (optional)
 * - Supports synchronous receive (receive) and asynchronous receive (message loop + callback)
 */
class MessageBusClient {
public:
    /**
     * Client configuration
     */
    struct Config {
        // Server endpoint
        std::string server_endpoint = "ipc:///tmp/moltcat_messagebus.sock";

        // Server broadcast endpoint (optional)
        std::string server_pub_endpoint = "ipc:///tmp/moltcat_messagebus_pub.sock";

        // Local queue ID (auto-generated or manually specified)
        std::string queue_id;  // If empty, auto-generate UUID

        // Service name
        std::string service_name = "UnknownService";

        // Connection timeout (milliseconds)
        int connect_timeout_ms = 5000;

        // I/O thread count (ZeroMQ internal)
        // Clients typically need fewer I/O threads
        // Single client recommendation: 1-2, increase appropriately for multi-connection scenarios
        int io_threads = 2;

        // Receive timeout (milliseconds, -1 means blocking)
        int recv_timeout_ms = -1;

        // Send timeout (milliseconds, -1 means blocking)
        int send_timeout_ms = -1;

        // Auto reconnect
        bool auto_reconnect = true;
        int reconnect_interval_ms = 1000;

        // Enable subscription broadcasts (requires PUB endpoint)
        bool enable_subscription = false;

        // Log level
        spdlog::level::level_enum log_level = spdlog::level::info;
    };

    explicit MessageBusClient(const Config& config);
    ~MessageBusClient();

    // Disable copy and move
    MessageBusClient(const MessageBusClient&) = delete;
    MessageBusClient& operator=(const MessageBusClient&) = delete;
    MessageBusClient(MessageBusClient&&) = delete;
    MessageBusClient& operator=(MessageBusClient&&) = delete;

    /**
     * Connect to server and register service
     *
     * @return true indicates successful connection
     */
    auto connect() -> bool;

    /**
     * Disconnect and unregister service
     */
    auto disconnect() -> void;

    /**
     * Check connection status
     */
    auto is_connected() const -> bool { return connected_; }

    // ==================== Message Sending API ====================

    /**
     * Send message (auto routing)
     *
     * @param msg Message
     * @param timeout_ms Timeout
     * @return true indicates successful send
     */
    auto send(const Message& msg, int timeout_ms = -1) -> bool;

    /**
     * Send to specific service
     *
     * @param service_name Service name
     * @param msg Message
     * @param timeout_ms Timeout
     * @return true indicates successful send
     */
    auto send_to(
        const std::string& service_name,
        const Message& msg,
        int timeout_ms = -1
    ) -> bool;

    /**
     * Broadcast message (send to all instances of service)
     *
     * @param service_name Service name
     * @param msg Message
     * @param timeout_ms Timeout
     * @return Number of instances successfully sent to
     */
    auto broadcast(
        const std::string& service_name,
        const Message& msg,
        int timeout_ms = -1
    ) -> size_t;

    // ==================== Subscription API ====================

    /**
     * Subscribe to topic
     *
     * @param topic Topic name
     */
    auto subscribe(const std::string& topic) -> void;

    /**
     * Unsubscribe
     *
     * @param topic Topic name
     */
    auto unsubscribe(const std::string& topic) -> void;

    // ==================== Message Receiving API ====================

    /**
     * Receive message (synchronous)
     *
     * @param timeout_ms Timeout (-1 means blocking)
     * @return Received message, returns nullopt on timeout
     */
    auto receive(int timeout_ms = -1) -> std::optional<Message>;

    /**
     * Start message loop (asynchronous receive)
     *
     * @param callback Message handling callback
     */
    auto start_message_loop(
        std::function<void(const Message&)> callback
    ) -> void;

    /**
     * Stop message loop
     */
    auto stop_message_loop() -> void;

    /**
     * Check if message loop is running
     */
    auto is_receiving() const -> bool { return receiving_.load(); }

    // ==================== Management Interface ====================

    /**
     * Get queue ID
     */
    auto get_queue_id() const -> const std::string& { return config_.queue_id; }

    /**
     * Get service name
     */
    auto get_service_name() const -> const std::string& { return config_.service_name; }

    /**
     * Get configuration
     */
    auto get_config() const -> const Config& { return config_; }

private:
    Config config_;
    bool connected_ = false;

    // ZeroMQ context
    zmq::context_t context_;

    // DEALER socket (connects to server's ROUTER)
    zmq::socket_t dealer_socket_;

    // SUB socket (subscribe to broadcasts)
    std::optional<zmq::socket_t> sub_socket_;

    // Message loop thread
    std::thread message_loop_thread_;
    std::atomic<bool> receiving_{false};

    // Logger
    std::shared_ptr<spdlog::logger> logger_;

    /**
     * Internal implementation methods
     */

    // Register service to server
    auto register_service() -> bool;

    // Unregister service
    auto unregister_service() -> bool;

    // Send internal message
    auto send_internal(
        const Message& msg,
        int timeout_ms
    ) -> bool;

    // Message loop (asynchronous receive)
    auto message_loop(std::function<void(const Message&)> callback) -> void;

    // Connect to server
    auto connect_to_server() -> bool;
};

} // namespace moltcat::messaging
