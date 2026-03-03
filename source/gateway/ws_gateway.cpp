#include "ws_gateway.hpp"
#include "../protocol/ws_frames.hpp"
#include "../protocol/ws_methods.hpp"
#include "../protocol/ws_schema.hpp"
#include "../utils/string_utils.hpp"
#include "../utils/uuid.hpp"
#include <uwebsockets/App.h>
#include <random>
#include <chrono>
#include <algorithm>

namespace moltcat::gateway {

// ==================== Construction and Destruction ====================

WsGateway::WsGateway(
    const WsGatewayConfig& config,
    std::shared_ptr<auth::AuthManager> auth_manager,
    std::shared_ptr<auth::PermissionChecker> permission_checker,
    std::shared_ptr<TaskManager> task_manager,
    messaging::MessageBusClient* message_bus_client
)
    : config_(config)
    , auth_manager_(std::move(auth_manager))
    , permission_checker_(std::move(permission_checker))
    , task_manager_(std::move(task_manager))
    , message_bus_client_(message_bus_client)
{
    if (!auth_manager_) {
        throw std::invalid_argument("AuthManager cannot be null");
    }

    if (!permission_checker_) {
        throw std::invalid_argument("PermissionChecker cannot be null");
    }

    if (!task_manager_) {
        throw std::invalid_argument("TaskManager cannot be null");
    }

    // Set MessageBusClient to TaskManager
    if (message_bus_client_) {
        task_manager_->set_message_bus_client(message_bus_client_);
    }

    // Initialize method handlers
    init_method_handlers();
}

WsGateway::~WsGateway() {
    stop();
}

// ==================== Start and Stop ====================

auto WsGateway::start() -> bool {
    if (running_.load()) {
        return false;  // Already running
    }

    try {
        // Create uWS application
        uws_app_ = std::make_unique<uWS::App>();

        // Setup WebSocket route
        uws_app_->ws<PerSocketData>(config_.path,
            // .open
            [this](WebSocket* ws) {
                this->on_open(ws);
            },
            // .message
            [this](WebSocket* ws, std::string_view message, uWS::OpCode op_code) {
                this->on_message(ws, message, op_code);
            },
            // .close
            [this](WebSocket* ws, int code, std::string_view reason) {
                this->on_close(ws, code, reason);
            }
        );

        // Start server
        uws_app_->listen(config_.port, [this](auto* token) {
            if (token) {
                std::cout << "WebSocket Gateway listening on " << config_.host
                          << ":" << config_.port << config_.path << std::endl;

                running_.store(true);

                // Start heartbeat thread
                start_heartbeat();
            } else {
                std::cerr << "WebSocket Gateway failed to start: port " << config_.port
                          << " already in use" << std::endl;
            }
        });

        // Run event loop
        uws_app_->run();

        return true;
    } catch (const std::exception& e) {
        std::cerr << "WebSocket Gateway startup exception: " << e.what() << std::endl;
        return false;
    }
}

auto WsGateway::stop() -> void {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    // Stop heartbeat thread
    {
        std::lock_guard lock(heartbeat_mutex_);
        heartbeat_cv_.notify_all();
    }

    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }

    // Close uWS application
    if (uws_app_) {
        // uWS will clean up automatically
    }

    std::cout << "WebSocket Gateway stopped" << std::endl;
}

// ==================== Initialize Method Handlers ====================

auto WsGateway::init_method_handlers() -> void {
    // Register all method handlers

    // connect method (special handling, not through generic routing)
    // handle_connect is called directly in on_message

    // ping method
    register_method_handler("ping", [this](const auto& params, const auto& conn) {
        return handle_ping(params, conn);
    });

    // system.presence method
    register_method_handler("system.presence", [this](const auto& params, const auto& conn) {
        return handle_system_presence(params, conn);
    });

    // system.status method
    register_method_handler("system.status", [this](const auto& params, const auto& conn) {
        return handle_system_status(params, conn);
    });

    // Task management methods
    register_method_handler("task.create", [this](const auto& params, const auto& conn) {
        return handle_task_create(params, conn);
    });

    register_method_handler("task.get", [this](const auto& params, const auto& conn) {
        return handle_task_get(params, conn);
    });

    register_method_handler("task.list", [this](const auto& params, const auto& conn) {
        return handle_task_list(params, conn);
    });

    register_method_handler("task.cancel", [this](const auto& params, const auto& conn) {
        return handle_task_cancel(params, conn);
    });
}

auto WsGateway::register_method_handler(std::string_view method, MethodHandler handler)
    -> void {
    method_handlers_[std::string(method)] = std::move(handler);
}

// ==================== uWS Callbacks ====================

auto WsGateway::on_open(WebSocket* ws) -> void {
    auto* data = static_cast<PerSocketData*>(ws->getUserData());
    data->connection_id = generate_connection_id();
    data->authenticated = false;

    std::cout << "Connection opened: " << data->connection_id << std::endl;

    // Send connection challenge
    if (config_.require_auth && config_.enable_device_auth) {
        send_challenge(data->connection_id, ws);
    } else {
        // No authentication required, mark as authenticated directly (not recommended for production)
        data->authenticated = true;
    }
}

auto WsGateway::on_message(WebSocket* ws, std::string_view message, uWS::OpCode op_code)
    -> void {
    // Only process text messages
    if (op_code != uWS::OpCode::TEXT) {
        return;
    }

    auto connection_id = get_connection_id(ws);
    process_request(connection_id, ws, message);
}

auto WsGateway::on_close(WebSocket* ws, int code, std::string_view reason) -> void {
    auto* data = static_cast<PerSocketData*>(ws->getUserData());
    auto connection_id = data->connection_id;

    std::cout << "Connection closed: " << connection_id << " (code=" << code
              << ", reason=" << reason << ")" << std::endl;

    // Remove from connection table
    {
        std::unique_lock lock(connections_mutex_);
        connections_.erase(connection_id);
        socket_map_.erase(connection_id);
    }
}

// ==================== Protocol Handling ====================

auto WsGateway::handle_connect(
    const glz::json_t& params,
    const std::string& connection_id,
    WebSocket* ws
) -> glz::json_t {
    // 1. Parse connect parameters
    auto connect_params = protocol::ConnectParams::from_json(params);
    if (!connect_params.has_value()) {
        return glz::json_t{
            {"type", "error"},
            {"code", -32602},
            {"message", "Invalid connect params"}
        };
    }

    auto& p = connect_params.value();

    // 2. Validate protocol version
    if (p.min_protocol > config_.max_protocol ||
        p.max_protocol < config_.min_protocol) {
        return glz::json_t{
            {"type", "error"},
            {"code", 1200},
            {"message", "Unsupported protocol version"}
        };
    }

    // 3. Validate role
    if (!protocol::SchemaValidator::validate_role(p.role)) {
        return glz::json_t{
            {"type", "error"},
            {"code", -32602},
            {"message", "Invalid role"}
        };
    }

    // 4. Get connection data
    auto* data = static_cast<PerSocketData*>(ws->getUserData());

    // 5. If device signature verification is enabled
    if (config_.enable_device_auth && p.device.has_value()) {
        auto& device = p.device.value();

        // Verify device signature
        if (!verify_device_signature(
            device.id,
            device.public_key,
            device.signature,
            data->current_nonce
        )) {
            return glz::json_t{
                {"type", "error"},
                {"code", 1101},
                {"message", "Invalid device signature"}
            };
        }

        // Register device
        auto register_result = auth_manager_->register_device(device.id, device.public_key);
        if (!register_result.has_value()) {
            return glz::json_t{
                {"type", "error"},
                {"code", 1103},
                {"message", "Device registration failed"}
            };
        }

        // Issue device token
        auto token_result = auth_manager_->issue_device_token(
            device.id,
            p.role,
            p.scopes
        );

        if (!token_result.has_value()) {
            return glz::json_t{
                {"type", "error"},
                {"code", -32603},
                {"message", "Failed to issue device token"}
            };
        }

        // 6. Create connection information
        ConnectionInfo conn;
        conn.connection_id = connection_id;
        conn.device_id = device.id;
        conn.role = p.role;
        conn.scopes = p.scopes;
        conn.caps = p.caps;
        conn.commands = p.commands;
        conn.permissions = p.permissions;
        conn.client_id = p.client.id;
        conn.client_version = p.client.version;
        conn.platform = p.client.platform;
        conn.connected_at = std::chrono::system_clock::now();
        conn.last_activity = std::chrono::system_clock::now();
        conn.protocol_version = config_.min_protocol;
        conn.device_token = token_result.value();

        // 7. Store connection
        {
            std::unique_lock lock(connections_mutex_);
            connections_[connection_id] = conn;
            socket_map_[connection_id] = ws;
        }

        // 8. Mark as authenticated
        data->authenticated = true;

        // 9. Build hello-ok response
        protocol::HelloOkPayload hello_payload;
        hello_payload.protocol = config_.min_protocol;
        hello_payload.policy.tick_interval_ms = config_.tick_interval_ms;
        hello_payload.auth.device_token = token_result.value();
        hello_payload.auth.role = p.role;
        hello_payload.auth.scopes = p.scopes;

        return hello_payload.to_json();
    } else {
        // Simplified authentication (using Bearer Token only)
        if (!p.auth.has_value()) {
            return glz::json_t{
                {"type", "error"},
                {"code", 1001},
                {"message", "Authentication required"}
            };
        }

        // Verify token
        auto token_info = auth_manager_->verify_token(p.auth.value().token);
        if (!token_info.has_value()) {
            return glz::json_t{
                {"type", "error"},
                {"code", 1001},
                {"message", "Invalid or expired token"}
            };
        }

        // Create connection information
        ConnectionInfo conn;
        conn.connection_id = connection_id;
        conn.device_id = token_info.value().device_id;
        conn.role = token_info.value().role;
        conn.scopes = token_info.value().scopes;
        conn.caps = token_info.value().caps;
        conn.commands = token_info.value().commands;
        conn.permissions = token_info.value().permissions;
        conn.client_id = p.client.id;
        conn.client_version = p.client.version;
        conn.platform = p.client.platform;
        conn.connected_at = std::chrono::system_clock::now();
        conn.last_activity = std::chrono::system_clock::now();
        conn.protocol_version = config_.min_protocol;
        conn.device_token = p.auth.value().token;

        // Store connection
        {
            std::unique_lock lock(connections_mutex_);
            connections_[connection_id] = conn;
            socket_map_[connection_id] = ws;
        }

        // Mark as authenticated
        data->authenticated = true;

        // Build hello-ok response
        protocol::HelloOkPayload hello_payload;
        hello_payload.protocol = config_.min_protocol;
        hello_payload.policy.tick_interval_ms = config_.tick_interval_ms;
        hello_payload.auth.device_token = p.auth.value().token;
        hello_payload.auth.role = token_info.value().role;
        hello_payload.auth.scopes = token_info.value().scopes;

        return hello_payload.to_json();
    }
}

auto WsGateway::handle_ping(
    const glz::json_t& params,
    const ConnectionInfo& conn
) -> glz::json_t {
    // Parse parameters
    auto ping_params = protocol::PingParams::from_json(params);
    if (!ping_params.has_value()) {
        return glz::json_t{
            {"type", "error"},
            {"code", -32602},
            {"message", "Invalid ping params"}
        };
    }

    // Calculate latency
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    uint64_t latency_ms = 0;
    if (ping_params.value().timestamp > 0) {
        latency_ms = now_ms - ping_params.value().timestamp;
    }

    // Build response
    protocol::PingPayload payload;
    payload.timestamp = now_ms;
    payload.latency_ms = latency_ms;

    return payload.to_json();
}

auto WsGateway::handle_system_presence(
    const glz::json_t& params,
    const ConnectionInfo& conn
) -> glz::json_t {
    // Check permissions
    if (!permission_checker_->check_scope(conn, "operator.read")) {
        return glz::json_t{
            {"type", "error"},
            {"code", 1002},
            {"message", "Insufficient permissions"}
        };
    }

    // Get all connections
    auto all_conns = get_all_connections();

    // Build response
    glz::json_t result;
    result["devices"] = glz::json_t::array();

    for (const auto& c : all_conns) {
        glz::json_t device_info;
        device_info["deviceId"] = c.device_id;
        device_info["connectionId"] = c.connection_id;
        device_info["role"] = c.role;
        device_info["scopes"] = c.scopes;
        device_info["client"] = c.client_id;
        device_info["platform"] = c.platform;

        auto connected_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            c.connected_at.time_since_epoch()
        ).count();
        device_info["connectedAt"] = connected_ms;

        auto activity_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            c.last_activity.time_since_epoch()
        ).count();
        device_info["lastSeen"] = activity_ms;

        result["devices"].push_back(device_info);
    }

    result["total"] = all_conns.size();

    return result;
}

auto WsGateway::handle_system_status(
    const glz::json_t& params,
    const ConnectionInfo& conn
) -> glz::json_t {
    // Check permissions
    if (!permission_checker_->check_scope(conn, "operator.read")) {
        return glz::json_t{
            {"type", "error"},
            {"code", 1002},
            {"message", "Insufficient permissions"}
        };
    }

    // Build system status
    glz::json_t result;
    result["status"] = "healthy";
    result["version"] = "1.0.0";
    result["uptime"] = 0;  // TODO: Implement uptime calculation

    glz::json_t connections;
    connections["total"] = get_connection_count();

    size_t operators = 0;
    size_t nodes = 0;

    auto all_conns = get_all_connections();
    for (const auto& c : all_conns) {
        if (c.role == "operator") {
            operators++;
        } else if (c.role == "node") {
            nodes++;
        }
    }

    connections["operators"] = operators;
    connections["nodes"] = nodes;

    result["connections"] = connections;

    // Get task statistics
    auto task_stats = task_manager_->get_statistics();
    result["tasks"] = task_stats;

    return result;
}

// ==================== Challenge-Response Mechanism ====================

auto WsGateway::send_challenge(const std::string& connection_id, WebSocket* ws)
    -> std::string {
    auto nonce = generate_nonce();

    // Store nonce
    auto* data = static_cast<PerSocketData*>(ws->getUserData());
    data->current_nonce = nonce;
    data->challenge_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    // Build challenge event
    protocol::ConnectChallengePayload payload;
    payload.nonce = nonce;
    payload.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    send_event_frame(ws, "connect.challenge", payload.to_json());

    return nonce;
}

auto WsGateway::generate_nonce() -> std::string {
    // Generate 32-byte random nonce
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::ostringstream oss;
    for (int i = 0; i < 32; ++i) {
        oss << std::hex << dis(gen);
    }

    return oss.str();
}

auto WsGateway::verify_device_signature(
    const std::string& device_id,
    const std::string& public_key,
    const std::string& signature,
    const std::string& nonce
) -> bool {
    // Build signature payload
    glz::json_t payload;
    payload["nonce"] = nonce;

    std::string payload_str;
    glz::write_json(payload, payload_str);

    // Verify using SignatureVerifier
    auth::SignatureVerifier verifier;
    auto result = verifier.verify_device_signature(
        device_id,
        payload_str,
        signature
    );

    return result.has_value() && result.value();
}

// ==================== Message Sending ====================

auto WsGateway::send_event_frame(
    WebSocket* ws,
    std::string_view event,
    const glz::json_t& payload,
    std::optional<uint64_t> seq
) -> bool {
    protocol::WsEventFrame frame;
    frame.event = event;
    frame.payload = payload;
    frame.seq = seq;

    auto json_str = frame.to_json();
    return send_json(ws, json_str);
}

auto WsGateway::send_response_frame(
    WebSocket* ws,
    std::string_view request_id,
    const glz::json_t& payload
) -> bool {
    auto frame = protocol::WsResponseFrame::success(request_id, payload);
    auto json_str = frame.to_json();
    return send_json(ws, json_str);
}

auto WsGateway::send_error_frame(
    WebSocket* ws,
    std::string_view request_id,
    int code,
    std::string_view message,
    const glz::json_t& details
) -> bool {
    auto frame = protocol::WsResponseFrame::error(request_id, code, message, details);
    auto json_str = frame.to_json();
    return send_json(ws, json_str);
}

auto WsGateway::send_json(WebSocket* ws, const std::string& json_str) -> bool {
    if (ws) {
        ws->send(json_str, uWS::OpCode::TEXT);
        return true;
    }
    return false;
}

// ==================== Request Processing ====================

auto WsGateway::process_request(
    const std::string& connection_id,
    WebSocket* ws,
    std::string_view message
) -> void {
    try {
        // Parse JSON
        glz::json_t json;
        auto err = glz::read_json(json, message);
        if (err) {
            send_error_frame(ws, "", -32700, "Parse error");
            return;
        }

        // Get type
        auto type_it = json.find("type");
        if (type_it == json.end()) {
            send_error_frame(ws, "", -32600, "Missing type field");
            return;
        }

        auto type = type_it->get<std::string>();

        if (type == "req") {
            // Request frame
            auto req = protocol::WsRequestFrame::from_json(message);
            if (!req.has_value()) {
                send_error_frame(ws, "", -32600, "Invalid request frame");
                return;
            }

            auto& request = req.value();

            // Check if authenticated (except for connect method)
            auto* data = static_cast<PerSocketData*>(ws->getUserData());
            if (!data->authenticated && request.method != "connect") {
                send_error_frame(ws, request.id, 1001, "Not authenticated");
                return;
            }

            // Update activity time
            {
                std::unique_lock lock(connections_mutex_);
                auto it = connections_.find(connection_id);
                if (it != connections_.end()) {
                    it->second.last_activity = std::chrono::system_clock::now();
                }
            }

            // Handle connect method (special handling)
            if (request.method == "connect") {
                auto result = handle_connect(request.params, connection_id, ws);

                // Check if error response
                auto type_it = result.find("type");
                if (type_it != result.end() && type_it->get<std::string>() == "error") {
                    send_error_frame(ws, request.id,
                        result["code"], result["message"]);
                } else {
                    send_response_frame(ws, request.id, result);
                }
                return;
            }

            // Handle other methods
            auto handler_it = method_handlers_.find(request.method);
            if (handler_it == method_handlers_.end()) {
                send_error_frame(ws, request.id, -32601,
                    std::format("Method not found: {}", request.method));
                return;
            }

            // Get connection information
            std::shared_lock lock(connections_mutex_);
            auto conn_it = connections_.find(connection_id);
            if (conn_it == connections_.end()) {
                send_error_frame(ws, request.id, -32603, "Connection not found");
                return;
            }

            const auto& conn = conn_it->second;
            lock.unlock();

            // Check permissions
            auto perm_check = permission_checker_->check_method_permission(
                conn, request.method
            );
            if (!perm_check.has_value()) {
                send_error_frame(ws, request.id, -32603, "Permission check failed");
                return;
            }

            if (!perm_check.value()) {
                send_error_frame(ws, request.id, 1002, "Insufficient permissions");
                return;
            }

            // Call method handler
            try {
                auto result = handler_it->second(request.params, conn);
                send_response_frame(ws, request.id, result);
            } catch (const std::exception& e) {
                send_error_frame(ws, request.id, -32603,
                    std::format("Internal error: {}", e.what()));
            }

        } else {
            send_error_frame(ws, "", -32600, "Unsupported message type");
        }

    } catch (const std::exception& e) {
        std::cerr << "Request processing exception: " << e.what() << std::endl;
        send_error_frame(ws, "", -32603, "Internal error");
    }
}

// ==================== Broadcasting and Sending ====================

auto WsGateway::broadcast_event(std::string_view event, const glz::json_t& payload)
    -> void {
    std::shared_lock lock(connections_mutex_);

    for (const auto& [conn_id, ws] : socket_map_) {
        send_event_frame(ws, event, payload);
    }
}

auto WsGateway::send_event_to(
    const std::string& connection_id,
    std::string_view event,
    const glz::json_t& payload
) -> bool {
    std::shared_lock lock(connections_mutex_);

    auto it = socket_map_.find(connection_id);
    if (it != socket_map_.end()) {
        return send_event_frame(it->second, event, payload);
    }

    return false;
}

// ==================== Connection Queries ====================

auto WsGateway::get_connection(const std::string& connection_id) const
    -> std::optional<ConnectionInfo> {
    std::shared_lock lock(connections_mutex_);

    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        return it->second;
    }

    return std::nullopt;
}

auto WsGateway::get_all_connections() const -> std::vector<ConnectionInfo> {
    std::shared_lock lock(connections_mutex_);

    std::vector<ConnectionInfo> result;
    result.reserve(connections_.size());

    for (const auto& [id, conn] : connections_) {
        result.push_back(conn);
    }

    return result;
}

auto WsGateway::get_connection_count() const -> size_t {
    std::shared_lock lock(connections_mutex_);
    return connections_.size();
}

// ==================== Heartbeat Mechanism ====================

auto WsGateway::start_heartbeat() -> void {
    heartbeat_thread_ = std::thread([this]() {
        heartbeat_loop();
    });
}

auto WsGateway::heartbeat_loop() -> void {
    while (running_.load()) {
        // Wait for tick_interval_ms or stop signal
        std::unique_lock lock(heartbeat_mutex_);
        heartbeat_cv_.wait_for(lock, std::chrono::milliseconds(config_.tick_interval_ms));

        if (!running_.load()) {
            break;
        }

        // Send tick
        send_tick();

        // Check for timeouts
        check_timeouts();
    }
}

auto WsGateway::send_tick() -> void {
    static uint64_t seq = 0;

    protocol::TickPayload payload;
    payload.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    payload.seq = ++seq;

    broadcast_event("tick", payload.to_json());
}

auto WsGateway::check_timeouts() -> void {
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> timed_out;

    {
        std::shared_lock lock(connections_mutex_);

        for (const auto& [conn_id, conn] : connections_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - conn.last_activity
            ).count();

            if (elapsed > static_cast<int64_t>(config_.heartbeat_timeout_ms)) {
                timed_out.push_back(conn_id);
            }
        }
    }

    // Close timeout connections
    for (const auto& conn_id : timed_out) {
        std::cout << "Connection timeout: " << conn_id << std::endl;

        std::unique_lock lock(connections_mutex_);
        auto it = socket_map_.find(conn_id);
        if (it != socket_map_.end()) {
            it->second->close(1000, "Heartbeat timeout");
            socket_map_.erase(it);
        }
        connections_.erase(conn_id);
    }
}

// ==================== Utility Methods ====================

auto WsGateway::generate_connection_id() -> std::string {
    return utils::UUID::generate();
}

auto WsGateway::get_connection_id(WebSocket* ws) -> std::string {
    auto* data = static_cast<PerSocketData*>(ws->getUserData());
    return data->connection_id;
}

// ==================== Task Management Methods ====================

auto WsGateway::handle_task_create(
    const glz::json_t& params,
    const ConnectionInfo& conn
) -> glz::json_t {
    // Check permissions
    if (!permission_checker_->check_command(conn, "task.execute")) {
        return glz::json_t{
            {"type", "error"},
            {"code", 1002},
            {"message", "Insufficient permissions: task.execute required"}
        };
    }

    // Parse parameters
    auto type_it = params.find("type");
    if (type_it == params.end() || !type_it->is_string()) {
        return glz::json_t{
            {"type", "error"},
            {"code", -32602},
            {"message", "Missing or invalid 'type' parameter"}
        };
    }

    std::string type = type_it->get<std::string>();

    // Get task parameters (optional)
    glz::json_t task_params;
    auto params_it = params.find("params");
    if (params_it != params.end()) {
        task_params = *params_it;
    }

    // Get idempotency key (optional)
    std::string idempotency_key;
    auto idem_it = params.find("idempotencyKey");
    if (idem_it != params.end() && idem_it->is_string()) {
        idempotency_key = idem_it->get<std::string>();
    }

    // Create task
    auto result = task_manager_->create_task(
        type,
        task_params,
        conn.device_id,
        conn.connection_id,
        idempotency_key
    );

    if (!result.has_value()) {
        return glz::json_t{
            {"type", "error"},
            {"code", result.error().code()},
            {"message", result.error().message()}
        };
    }

    // Return task information
    glz::json_t response;
    response["taskId"] = result.value();
    response["status"] = "pending";

    return response;
}

auto WsGateway::handle_task_get(
    const glz::json_t& params,
    const ConnectionInfo& conn
) -> glz::json_t {
    // Parse task ID
    auto id_it = params.find("taskId");
    if (id_it == params.end() || !id_it->is_string()) {
        return glz::json_t{
            {"type", "error"},
            {"code", -32602},
            {"message", "Missing or invalid 'taskId' parameter"}
        };
    }

    std::string task_id = id_it->get<std::string>();

    // Get task
    auto task_opt = task_manager_->get_task(task_id);
    if (!task_opt.has_value()) {
        return glz::json_t{
            {"type", "error"},
            {"code", ErrorCode::NOT_FOUND},
            {"message", std::format("Task not found: {}", task_id)}
        };
    }

    // Check permissions (can only view tasks created by self)
    if (task_opt.value().creator_device_id != conn.device_id &&
        !permission_checker_->check_scope(conn, "operator.read")) {
        return glz::json_t{
            {"type", "error"},
            {"code", 1002},
            {"message", "Insufficient permissions"}
        };
    }

    return task_opt.value().to_json();
}

auto WsGateway::handle_task_list(
    const glz::json_t& params,
    const ConnectionInfo& conn
) -> glz::json_t {
    // Check permissions
    if (!permission_checker_->check_scope(conn, "operator.read")) {
        return glz::json_t{
            {"type", "error"},
            {"code", 1002},
            {"message", "Insufficient permissions"}
        };
    }

    // Parse filter conditions (optional)
    std::optional<TaskStatus> status_filter;
    auto status_it = params.find("status");
    if (status_it != params.end() && status_it->is_string()) {
        std::string status_str = status_it->get<std::string>();
        if (status_str == "pending") {
            status_filter = TaskStatus::PENDING;
        } else if (status_str == "running") {
            status_filter = TaskStatus::RUNNING;
        } else if (status_str == "completed") {
            status_filter = TaskStatus::COMPLETED;
        } else if (status_str == "failed") {
            status_filter = TaskStatus::FAILED;
        } else if (status_str == "cancelled") {
            status_filter = TaskStatus::CANCELLED;
        }
    }

    std::optional<size_t> limit;
    auto limit_it = params.find("limit");
    if (limit_it != params.end() && limit_it->is_number()) {
        limit = limit_it->get<size_t>();
    }

    std::optional<size_t> offset;
    auto offset_it = params.find("offset");
    if (offset_it != params.end() && offset_it->is_number()) {
        offset = offset_it->get<size_t>();
    }

    // Get task list
    auto tasks = task_manager_->list_tasks(status_filter, limit, offset);

    // Build response
    glz::json_t result;
    result["tasks"] = glz::json_t::array();

    for (const auto& task : tasks) {
        result["tasks"].push_back(task.to_json());
    }

    result["total"] = tasks.size();
    if (limit.has_value()) {
        result["limit"] = limit.value();
    }
    if (offset.has_value()) {
        result["offset"] = offset.value();
    }

    return result;
}

auto WsGateway::handle_task_cancel(
    const glz::json_t& params,
    const ConnectionInfo& conn
) -> glz::json_t {
    // Parse task ID
    auto id_it = params.find("taskId");
    if (id_it == params.end() || !id_it->is_string()) {
        return glz::json_t{
            {"type", "error"},
            {"code", -32602},
            {"message", "Missing or invalid 'taskId' parameter"}
        };
    }

    std::string task_id = id_it->get<std::string>();

    // Get task to check permissions
    auto task_opt = task_manager_->get_task(task_id);
    if (!task_opt.has_value()) {
        return glz::json_t{
            {"type", "error"},
            {"code", ErrorCode::NOT_FOUND},
            {"message", std::format("Task not found: {}", task_id)}
        };
    }

    // Check permissions (can only cancel tasks created by self, unless has operator.write permission)
    if (task_opt.value().creator_device_id != conn.device_id &&
        !permission_checker_->check_scope(conn, "operator.write")) {
        return glz::json_t{
            {"type", "error"},
            {"code", 1002},
            {"message", "Insufficient permissions"}
        };
    }

    // Get cancellation reason (optional)
    std::string reason;
    auto reason_it = params.find("reason");
    if (reason_it != params.end() && reason_it->is_string()) {
        reason = reason_it->get<std::string>();
    }

    // Cancel task
    auto result = task_manager_->cancel_task(task_id, conn.device_id, reason);
    if (!result.has_value()) {
        return glz::json_t{
            {"type", "error"},
            {"code", result.error().code()},
            {"message", result.error().message()}
        };
    }

    // Return cancellation result
    glz::json_t response;
    response["taskId"] = task_id;
    response["status"] = "cancelled";

    return response;
}

} // namespace moltcat::gateway
