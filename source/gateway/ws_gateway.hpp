#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "connection_info.hpp"
#include "auth/auth_manager.hpp"
#include "auth/permission_checker.hpp"
#include "task_manager.hpp"
#include "../messaging/message_bus.hpp"
#include "../utils/error.hpp"

// uWebSockets forward declarations
namespace uWS {
    template<bool, bool, typename> struct WebSocketProtocol;
    template<typename> struct HttpResponse;
    template<typename> struct HttpRequest;
    template<bool SSL, typename USERDATA> struct WebSocketContext;
    template<typename T> struct WebSocket;

    struct App;
}

using WebSocket = uWS::WebSocket<false, true, PerSocketData>;
using AppState = uWS::TemplatedApp<false>;

namespace moltcat::gateway {

/**
 * @brief WebSocket Gateway configuration
 */
struct WsGatewayConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8081;
    std::string path = "/ws";

    // Protocol version
    uint32_t min_protocol = 1;
    uint32_t max_protocol = 1;

    // Heartbeat configuration
    size_t tick_interval_ms = 15000;       // Server heartbeat interval
    size_t heartbeat_timeout_ms = 60000;   // Client heartbeat timeout

    // Authentication configuration
    bool require_auth = true;
    bool enable_device_auth = true;
    bool allow_local_auto_approval = false;

    // TLS configuration (not yet supported)
    bool enable_tls = false;
    std::string cert_file;
    std::string private_key_file;
};

/**
 * @brief Per-Socket data
 */
struct PerSocketData {
    std::string connection_id;
    bool authenticated = false;
    std::string current_nonce;  // Current challenge nonce
    uint64_t challenge_time = 0;
};

/**
 * @brief Method handler type
 */
using MethodHandler = std::function<glz::json_t(
    const glz::json_t& params,    // Method parameters
    const ConnectionInfo& conn     // Connection information
)>;

/**
 * @brief WebSocket Gateway
 *
 * Implements complete OpenClaw WebSocket protocol
 */
class WsGateway {
public:
    /**
     * @brief Constructor
     *
     * @param config Configuration
     * @param auth_manager Authentication manager
     * @param permission_checker Permission checker
     * @param task_manager Task manager
     * @param message_bus_client Message bus client (optional, pointer, non-owning)
     */
    WsGateway(
        const WsGatewayConfig& config,
        std::shared_ptr<auth::AuthManager> auth_manager,
        std::shared_ptr<auth::PermissionChecker> permission_checker,
        std::shared_ptr<TaskManager> task_manager,
        messaging::MessageBusClient* message_bus_client = nullptr
    );

    /**
     * @brief Destructor
     */
    ~WsGateway();

    // Disable copy
    WsGateway(const WsGateway&) = delete;
    WsGateway& operator=(const WsGateway&) = delete;

    /**
     * @brief Start Gateway
     *
     * @return bool Whether startup succeeded
     */
    auto start() -> bool;

    /**
     * @brief Stop Gateway
     */
    auto stop() -> void;

    /**
     * @brief Broadcast event to all connections
     *
     * @param event Event name
     * @param payload Event payload
     */
    auto broadcast_event(std::string_view event, const glz::json_t& payload)
        -> void;

    /**
     * @brief Send event to specific connection
     *
     * @param connection_id Connection ID
     * @param event Event name
     * @param payload Event payload
     * @return bool Whether send succeeded
     */
    auto send_event_to(
        const std::string& connection_id,
        std::string_view event,
        const glz::json_t& payload
    ) -> bool;

    /**
     * @brief Get connection information
     *
     * @param connection_id Connection ID
     * @return std::optional<ConnectionInfo> Connection information, returns nullopt if not exists
     */
    [[nodiscard]] auto get_connection(const std::string& connection_id) const
        -> std::optional<ConnectionInfo>;

    /**
     * @brief Get all connections
     *
     * @return std::vector<ConnectionInfo> All connection information
     */
    [[nodiscard]] auto get_all_connections() const -> std::vector<ConnectionInfo>;

    /**
     * @brief Get online device count
     *
     * @return size_t Online device count
     */
    [[nodiscard]] auto get_connection_count() const -> size_t;

private:
    WsGatewayConfig config_;
    std::shared_ptr<auth::AuthManager> auth_manager_;
    std::shared_ptr<auth::PermissionChecker> permission_checker_;
    std::shared_ptr<TaskManager> task_manager_;
    messaging::MessageBusClient* message_bus_client_{nullptr};  // Non-owning, externally managed

    // uWS application
    std::unique_ptr<uWS::App> uws_app_;
    std::unique_ptr<uWS::Loop> loop_;

    // Connection management
    std::unordered_map<std::string, ConnectionInfo> connections_;
    std::unordered_map<std::string, WebSocket*> socket_map_;  // connection_id -> socket
    mutable std::shared_mutex connections_mutex_;

    // Method handlers
    std::unordered_map<std::string, MethodHandler> method_handlers_;

    // Heartbeat
    std::thread heartbeat_thread_;
    std::atomic<bool> running_{false};
    std::mutex heartbeat_mutex_;
    std::condition_variable heartbeat_cv_;

    // ==================== Initialization Methods ====================

    /**
     * @brief Initialize method handlers
     */
    auto init_method_handlers() -> void;

    /**
     * @brief Register method handler
     */
    auto register_method_handler(std::string_view method, MethodHandler handler)
        -> void;

    // ==================== uWS Callbacks ====================

    /**
     * @brief WebSocket connection open callback
     */
    auto on_open(WebSocket* ws) -> void;

    /**
     * @brief WebSocket message receive callback
     */
    auto on_message(WebSocket* ws, std::string_view message, uWS::OpCode op_code)
        -> void;

    /**
     * @brief WebSocket connection close callback
     */
    auto on_close(WebSocket* ws, int code, std::string_view reason) -> void;

    // ==================== Protocol Handling ====================

    /**
     * @brief Handle connect method
     */
    auto handle_connect(
        const glz::json_t& params,
        const std::string& connection_id,
        WebSocket* ws
    ) -> glz::json_t;

    /**
     * @brief Handle ping method
     */
    auto handle_ping(
        const glz::json_t& params,
        const ConnectionInfo& conn
    ) -> glz::json_t;

    /**
     * @brief Handle system.presence method
     */
    auto handle_system_presence(
        const glz::json_t& params,
        const ConnectionInfo& conn
    ) -> glz::json_t;

    /**
     * @brief Handle system.status method
     */
    auto handle_system_status(
        const glz::json_t& params,
        const ConnectionInfo& conn
    ) -> glz::json_t;

    // ==================== Task Management Methods ====================

    /**
     * @brief Handle task.create method
     */
    auto handle_task_create(
        const glz::json_t& params,
        const ConnectionInfo& conn
    ) -> glz::json_t;

    /**
     * @brief Handle task.get method
     */
    auto handle_task_get(
        const glz::json_t& params,
        const ConnectionInfo& conn
    ) -> glz::json_t;

    /**
     * @brief Handle task.list method
     */
    auto handle_task_list(
        const glz::json_t& params,
        const ConnectionInfo& conn
    ) -> glz::json_t;

    /**
     * @brief Handle task.cancel method
     */
    auto handle_task_cancel(
        const glz::json_t& params,
        const ConnectionInfo& conn
    ) -> glz::json_t;

    // ==================== Challenge-Response Mechanism ====================

    /**
     * @brief Send connection challenge
     */
    auto send_challenge(const std::string& connection_id, WebSocket* ws)
        -> std::string;

    /**
     * @brief Generate random nonce
     */
    [[nodiscard]] auto generate_nonce() -> std::string;

    /**
     * @brief Verify device signature
     */
    [[nodiscard]] auto verify_device_signature(
        const std::string& device_id,
        const std::string& public_key,
        const std::string& signature,
        const std::string& nonce
    ) -> bool;

    // ==================== Message Sending ====================

    /**
     * @brief Send event frame
     */
    auto send_event_frame(
        WebSocket* ws,
        std::string_view event,
        const glz::json_t& payload,
        std::optional<uint64_t> seq = std::nullopt
    ) -> bool;

    /**
     * @brief Send response frame
     */
    auto send_response_frame(
        WebSocket* ws,
        std::string_view request_id,
        const glz::json_t& payload
    ) -> bool;

    /**
     * @brief Send error response frame
     */
    auto send_error_frame(
        WebSocket* ws,
        std::string_view request_id,
        int code,
        std::string_view message,
        const glz::json_t& details = {}
    ) -> bool;

    // ==================== Heartbeat Mechanism ====================

    /**
     * @brief Start heartbeat thread
     */
    auto start_heartbeat() -> void;

    /**
     * @brief Heartbeat thread main function
     */
    auto heartbeat_loop() -> void;

    /**
     * @brief Send tick event to all connections
     */
    auto send_tick() -> void;

    /**
     * @brief Check timeout connections
     */
    auto check_timeouts() -> void;

    // ==================== Utility Methods ====================

    /**
     * @brief Generate connection ID
     */
    [[nodiscard]] auto generate_connection_id() -> std::string;

    /**
     * @brief Get connection ID from WebSocket
     */
    [[nodiscard]] auto get_connection_id(WebSocket* ws) -> std::string;

    /**
     * @brief Send JSON message
     */
    auto send_json(WebSocket* ws, const std::string& json_str) -> bool;

    /**
     * @brief Parse and process request
     */
    auto process_request(
        const std::string& connection_id,
        WebSocket* ws,
        std::string_view message
    ) -> void;
};

} // namespace moltcat::gateway
