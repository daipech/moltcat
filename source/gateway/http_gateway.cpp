#include "http_gateway.hpp"
#include "../protocol/ws_frames.hpp"
#include "../utils/string_utils.hpp"
#include <httplib.h>
#include <random>
#include <chrono>
#include <algorithm>

namespace moltcat::gateway {

// ==================== Constructor and Destructor ====================

HttpGateway::HttpGateway(
    const HttpGatewayConfig& config,
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

    server_ = std::make_unique<httplib::Server>();
}

HttpGateway::~HttpGateway() {
    stop();
}

// ==================== Start and Stop ====================

auto HttpGateway::start() -> bool {
    if (running_.load()) {
        return false;
    }

    try {
        // Setup routes
        setup_routes();

        // Start server in background thread
        server_thread_ = std::thread([this]() {
            if (!server_->listen(config_.host, config_.port)) {
                std::cerr << "HTTP Gateway failed to start: port " << config_.port
                          << " may already be in use" << std::endl;
            }
        });

        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (server_->is_running()) {
            running_.store(true);
            std::cout << "HTTP Gateway listening on http://" << config_.host
                      << ":" << config_.port << config_.base_path << std::endl;
            return true;
        }

        return false;
    } catch (const std::exception& e) {
        std::cerr << "HTTP Gateway startup exception: " << e.what() << std::endl;
        return false;
    }
}

auto HttpGateway::stop() -> void {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (server_) {
        server_->stop();
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    std::cout << "HTTP Gateway stopped" << std::endl;
}

auto HttpGateway::is_running() const -> bool {
    return running_.load() && server_ && server_->is_running();
}

// ==================== Route Setup ====================

auto HttpGateway::setup_routes() -> void {
    const std::string& base = config_.base_path;

    // ==================== System Status ====================

    // GET /api/v1/system/status
    server_->get(base + "/system/status",
        [this](const HttpRequest& req, HttpResponse& res) {
            // Authentication middleware
            if (!auth_middleware(req, res)) {
                return;
            }

            // Extract token
            auto token_opt = extract_bearer_token(req);
            if (!token_opt.has_value()) {
                send_error(res, 401, "Missing Authorization header");
                return;
            }

            // Verify token
            auto token_result = auth_manager_->verify_token(token_opt.value());
            if (!token_result.has_value()) {
                send_error(res, 401, "Invalid or expired token");
                return;
            }

            auto& token = token_result.value();

            // Handle request
            auto result = handle_system_status(req, token);
            send_json(res, result);
        }
    );

    // GET /api/v1/system/presence
    server_->get(base + "/system/presence",
        [this](const HttpRequest& req, HttpResponse& res) {
            if (!auth_middleware(req, res)) {
                return;
            }

            auto token_opt = extract_bearer_token(req);
            if (!token_opt.has_value()) {
                send_error(res, 401, "Missing Authorization header");
                return;
            }

            auto token_result = auth_manager_->verify_token(token_opt.value());
            if (!token_result.has_value()) {
                send_error(res, 401, "Invalid or expired token");
                return;
            }

            auto& token = token_result.value();

            auto result = handle_system_presence(req, token);
            send_json(res, result);
        }
    );

    // ==================== Task Management ====================

    // POST /api/v1/tasks
    server_->post(base + "/tasks",
        [this](const HttpRequest& req, HttpResponse& res) {
            if (!auth_middleware(req, res)) {
                return;
            }

            auto token_opt = extract_bearer_token(req);
            if (!token_opt.has_value()) {
                send_error(res, 401, "Missing Authorization header");
                return;
            }

            auto token_result = auth_manager_->verify_token(token_opt.value());
            if (!token_result.has_value()) {
                send_error(res, 401, "Invalid or expired token");
                return;
            }

            auto& token = token_result.value();

            // Idempotency middleware
            idempotency_middleware(req, res, [&]() {
                return handle_create_task(req, token);
            });
        }
    );

    // GET /api/v1/tasks/:task_id
    server_->get(base + "/tasks/.*",
        [this](const HttpRequest& req, HttpResponse& res) {
            if (!auth_middleware(req, res)) {
                return;
            }

            auto token_opt = extract_bearer_token(req);
            if (!token_opt.has_value()) {
                send_error(res, 401, "Missing Authorization header");
                return;
            }

            auto token_result = auth_manager_->verify_token(token_opt.value());
            if (!token_result.has_value()) {
                send_error(res, 401, "Invalid or expired token");
                return;
            }

            auto& token = token_result.value();

            auto result = handle_get_task(req, token);
            send_json(res, result);
        }
    );

    // GET /api/v1/tasks
    server_->get(base + "/tasks",
        [this](const HttpRequest& req, HttpResponse& res) {
            if (!auth_middleware(req, res)) {
                return;
            }

            auto token_opt = extract_bearer_token(req);
            if (!token_opt.has_value()) {
                send_error(res, 401, "Missing Authorization header");
                return;
            }

            auto token_result = auth_manager_->verify_token(token_opt.value());
            if (!token_result.has_value()) {
                send_error(res, 401, "Invalid or expired token");
                return;
            }

            auto& token = token_result.value();

            auto result = handle_list_tasks(req, token);
            send_json(res, result);
        }
    );

    // POST /api/v1/tasks/:task_id/cancel
    server_->post(base + "/tasks/.*/cancel",
        [this](const HttpRequest& req, HttpResponse& res) {
            if (!auth_middleware(req, res)) {
                return;
            }

            auto token_opt = extract_bearer_token(req);
            if (!token_opt.has_value()) {
                send_error(res, 401, "Missing Authorization header");
                return;
            }

            auto token_result = auth_manager_->verify_token(token_opt.value());
            if (!token_result.has_value()) {
                send_error(res, 401, "Invalid or expired token");
                return;
            }

            auto& token = token_result.value();

            auto result = handle_cancel_task(req, token);
            send_json(res, result);
        }
    );

    // ==================== Agent Management ====================

    // GET /api/v1/agents
    server_->get(base + "/agents",
        [this](const HttpRequest& req, HttpResponse& res) {
            if (!auth_middleware(req, res)) {
                return;
            }

            auto token_opt = extract_bearer_token(req);
            if (!token_opt.has_value()) {
                send_error(res, 401, "Missing Authorization header");
                return;
            }

            auto token_result = auth_manager_->verify_token(token_opt.value());
            if (!token_result.has_value()) {
                send_error(res, 401, "Invalid or expired token");
                return;
            }

            auto& token = token_result.value();

            auto result = handle_list_agents(req, token);
            send_json(res, result);
        }
    );

    // GET /api/v1/agents/:agent_id
    server_->get(base + "/agents/.*",
        [this](const HttpRequest& req, HttpResponse& res) {
            if (!auth_middleware(req, res)) {
                return;
            }

            auto token_opt = extract_bearer_token(req);
            if (!token_opt.has_value()) {
                send_error(res, 401, "Missing Authorization header");
                return;
            }

            auto token_result = auth_manager_->verify_token(token_opt.value());
            if (!token_result.has_value()) {
                send_error(res, 401, "Invalid or expired token");
                return;
            }

            auto& token = token_result.value();

            auto result = handle_get_agent(req, token);
            send_json(res, result);
        }
    );

    // POST /api/v1/agents
    server_->post(base + "/agents",
        [this](const HttpRequest& req, HttpResponse& res) {
            if (!auth_middleware(req, res)) {
                return;
            }

            auto token_opt = extract_bearer_token(req);
            if (!token_opt.has_value()) {
                send_error(res, 401, "Missing Authorization header");
                return;
            }

            auto token_result = auth_manager_->verify_token(token_opt.value());
            if (!token_result.has_value()) {
                send_error(res, 401, "Invalid or expired token");
                return;
            }

            auto& token = token_result.value();

            auto result = handle_register_agent(req, token);
            send_json(res, result);
        }
    );

    // ==================== OpenAI Compatible API ====================

    if (config_.enable_openai_api) {
        const std::string& openai_base = config_.openai_base_path;

        // POST /v1/chat/completions
        server_->post(openai_base + "/chat/completions",
            [this](const HttpRequest& req, HttpResponse& res) {
                // Authentication middleware
                if (!auth_middleware(req, res)) {
                    return;
                }

                // Extract token
                auto token_opt = extract_bearer_token(req);
                if (!token_opt.has_value()) {
                    send_error(res, 401, "Missing Authorization header");
                    return;
                }

                // Verify token
                auto token_result = auth_manager_->verify_token(token_opt.value());
                if (!token_result.has_value()) {
                    send_error(res, 401, "Invalid or expired token");
                    return;
                }

                auto& token = token_result.value();

                // Handle request
                auto result = handle_chat_completions(req, token);
                send_json(res, result);
            }
        );
    }

    // TODO: Add more endpoints (Phase 3.1+)
}

// ==================== Authentication Middleware ====================

auto HttpGateway::auth_middleware(const HttpRequest& req, HttpResponse& res) -> bool {
    if (!config_.require_auth) {
        return true;  // No authentication required
    }

    // Check Authorization header
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
        send_error(res, 401, "Missing Authorization header");
        return false;
    }

    // Check Bearer Token format
    if (auth_header.size() < 7 ||
        auth_header.substr(0, 7) != "Bearer ") {
        send_error(res, 401, "Invalid Authorization format. Use: Bearer <token>");
        return false;
    }

    return true;
}

auto HttpGateway::extract_bearer_token(const HttpRequest& req) const
    -> std::optional<std::string> {
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty() || auth_header.size() < 7) {
        return std::nullopt;
    }

    if (auth_header.substr(0, 7) != "Bearer ") {
        return std::nullopt;
    }

    return auth_header.substr(7);  // Remove "Bearer " prefix
}

// ==================== Idempotency Middleware ====================

auto HttpGateway::idempotency_middleware(
    const HttpRequest& req,
    HttpResponse& res,
    std::function<HttpResult()> handler
) -> void {
    if (!config_.enable_idempotency) {
        auto result = handler();
        send_json(res, result);
        return;
    }

    // Get idempotency key
    auto idempotency_key = req.get_header_value("Idempotency-Key");
    if (idempotency_key.empty()) {
        auto result = handler();
        send_json(res, result);
        return;
    }

    // Check if already exists
    {
        std::lock_guard lock(idempotency_mutex_);
        auto it = idempotency_cache_.find(idempotency_key);
        if (it != idempotency_cache_.end()) {
            // Return cached response
            res.set_status(it->second.status_code);
            res.set_content(it->second.response_body, it->second.content_type);
            return;
        }
    }

    // Execute handler
    auto result = handler();

    // Cache response
    {
        std::lock_guard lock(idempotency_mutex_);

        std::string body;
        glz::write_json(result.body, body);

        IdempotencyRecord record;
        record.response_body = body;
        record.status_code = result.status_code;
        record.content_type = result.content_type;
        record.created_at = std::chrono::system_clock::now();

        idempotency_cache_[idempotency_key] = record;
    }

    send_json(res, result);

    // Clean up expired keys
    cleanup_idempotency_keys();
}

auto HttpGateway::cleanup_idempotency_keys() -> void {
    auto now = std::chrono::system_clock::now();
    auto ttl = std::chrono::seconds(config_.idempotency_ttl_seconds);

    std::lock_guard lock(idempotency_mutex_);

    auto it = idempotency_cache_.begin();
    while (it != idempotency_cache_.end()) {
        auto age = now - it->second.created_at;
        if (age > ttl) {
            it = idempotency_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

// ==================== Rate Limiting Middleware ====================

auto HttpGateway::rate_limit_middleware(const HttpRequest& req, HttpResponse& res) -> bool {
    if (!config_.enable_rate_limit) {
        return true;
    }

    // TODO: Implement rate limiting logic
    return true;
}

// ==================== API Endpoint Handlers ====================

auto HttpGateway::handle_system_status(const HttpRequest& req, const token::TokenInfo& token)
    -> HttpResult {
    // Check permissions
    if (!check_permission(token, "operator.read")) {
        return HttpResult::error(403, "Insufficient permissions");
    }

    // Get task statistics
    auto task_stats = task_manager_->get_statistics();

    // TODO: Integrate actual connection statistics
    glz::json_t result;
    result["ok"] = true;
    result["result"] = glz::json_t{
        {"status", "healthy"},
        {"version", "1.0.0"},
        {"uptime", 0},
        {"connections", glz::json_t{
            {"operators", 0},
            {"nodes", 0}
        }},
        {"tasks", task_stats}
    };

    return HttpResult::ok(result);
}

auto HttpGateway::handle_system_presence(const HttpRequest& req, const token::TokenInfo& token)
    -> HttpResult {
    // Check permissions
    if (!check_permission(token, "operator.read")) {
        return HttpResult::error(403, "Insufficient permissions");
    }

    // TODO: Get online devices from WsGateway
    glz::json_t result;
    result["ok"] = true;
    result["result"] = glz::json_t{
        {"devices", glz::json_t::array()},
        {"total", 0}
    };

    return HttpResult::ok(result);
}

auto HttpGateway::handle_create_task(const HttpRequest& req, const token::TokenInfo& token)
    -> HttpResult {
    // Check permissions
    if (!check_permission(token, "task.execute")) {
        return HttpResult::error(403, "Insufficient permissions: task.execute required");
    }

    // Parse request body
    auto body_opt = parse_json_body(req);
    if (!body_opt.has_value()) {
        return HttpResult::error(400, "Invalid JSON body");
    }

    auto& body = body_opt.value();

    // Validate required fields
    if (!body.contains("type") || !body["type"].is_string()) {
        return HttpResult::error(400, "Missing or invalid required field: type",
            glz::json_t{{"field", "type"}, {"reason", "required string"}});
    }

    std::string type = body["type"].get<std::string>();

    // Get task parameters (optional)
    glz::json_t task_params;
    if (body.contains("params")) {
        task_params = body["params"];
    }

    // Get idempotency key (optional)
    std::string idempotency_key;
    if (req.has_header("Idempotency-Key")) {
        idempotency_key = req.get_header_value("Idempotency-Key");
    }

    // Create task
    auto result = task_manager_->create_task(
        type,
        task_params,
        token.device_id,
        "",  // HTTP request has no connection_id
        idempotency_key
    );

    if (!result.has_value()) {
        return HttpResult::error(
            result.error().code() >= 400 ? result.error().code() : 500,
            result.error().message()
        );
    }

    glz::json_t response;
    response["ok"] = true;
    response["result"] = glz::json_t{
        {"taskId", result.value()},
        {"status", "pending"},
        {"createdAt", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()}
    };

    return HttpResult::created(response);
}

auto HttpGateway::handle_get_task(const HttpRequest& req, const token::TokenInfo& token)
    -> HttpResult {
    // Check permissions
    if (!check_permission(token, "operator.read")) {
        return HttpResult::error(403, "Insufficient permissions");
    }

    // Extract task_id from path
    auto path = req.path;
    auto base_pos = path.find(config_.base_path + "/tasks/");
    if (base_pos == std::string::npos) {
        return HttpResult::error(400, "Invalid task ID");
    }

    std::string task_id = path.substr(base_pos + config_.base_path.size() + 8);  // "/tasks/"

    // Query task
    auto task_opt = task_manager_->get_task(task_id);
    if (!task_opt.has_value()) {
        glz::json_t result;
        result["ok"] = false;
        result["error"] = glz::json_t{
            {"code", ErrorCode::NOT_FOUND},
            {"message", std::format("Task not found: {}", task_id)}
        };
        return HttpResult::ok(result);
    }

    // Check permissions (can only view tasks created by self)
    if (task_opt.value().creator_device_id != token.device_id) {
        return HttpResult::error(403, "Insufficient permissions");
    }

    glz::json_t result;
    result["ok"] = true;
    result["result"] = task_opt.value().to_json();

    return HttpResult::ok(result);
}

auto HttpGateway::handle_list_tasks(const HttpRequest& req, const token::TokenInfo& token)
    -> HttpResult {
    // Check permissions
    if (!check_permission(token, "operator.read")) {
        return HttpResult::error(403, "Insufficient permissions");
    }

    // Parse query parameters
    auto status_str = req.get_param_value("status");
    auto limit_str = req.get_param_value("limit");
    auto offset_str = req.get_param_value("offset");

    std::optional<TaskStatus> status_filter;
    if (!status_str.empty()) {
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
    if (!limit_str.empty()) {
        limit = std::stoul(limit_str);
    }

    std::optional<size_t> offset;
    if (!offset_str.empty()) {
        offset = std::stoul(offset_str);
    }

    // Query task list
    auto tasks = task_manager_->list_tasks(status_filter, limit, offset);

    glz::json_t result;
    result["ok"] = true;
    result["result"] = glz::json_t{
        {"tasks", glz::json_t::array()},
        {"total", tasks.size()}
    };

    if (limit.has_value()) {
        result["result"]["limit"] = limit.value();
    }
    if (offset.has_value()) {
        result["result"]["offset"] = offset.value();
    }

    for (const auto& task : tasks) {
        result["result"]["tasks"].push_back(task.to_json());
    }

    return HttpResult::ok(result);
}

auto HttpGateway::handle_cancel_task(const HttpRequest& req, const token::TokenInfo& token)
    -> HttpResult {
    // Check permissions
    if (!check_permission(token, "task.execute")) {
        return HttpResult::error(403, "Insufficient permissions: task.execute required");
    }

    // Extract task_id from path
    auto path = req.path;
    auto base_pos = path.find(config_.base_path + "/tasks/");
    if (base_pos == std::string::npos) {
        return HttpResult::error(400, "Invalid task ID");
    }

    // Extract task_id (remove "/cancel" suffix)
    auto task_path = path.substr(base_pos);
    auto cancel_pos = task_path.find("/cancel");
    if (cancel_pos != std::string::npos) {
        task_path = task_path.substr(0, cancel_pos);
    }
    std::string task_id = task_path;

    // Check if task exists and permissions
    auto task_opt = task_manager_->get_task(task_id);
    if (!task_opt.has_value()) {
        return HttpResult::error(404, std::format("Task not found: {}", task_id));
    }

    // Check permissions (can only cancel tasks created by self, unless has operator.write permission)
    if (task_opt.value().creator_device_id != token.device_id &&
        !check_permission(token, "operator.write")) {
        return HttpResult::error(403, "Insufficient permissions");
    }

    // Parse cancellation reason (optional)
    std::string reason;
    auto body_opt = parse_json_body(req);
    if (body_opt.has_value()) {
        auto& body = body_opt.value();
        if (body.contains("reason") && body["reason"].is_string()) {
            reason = body["reason"].get<std::string>();
        }
    }

    // Cancel task
    auto result = task_manager_->cancel_task(task_id, token.device_id, reason);
    if (!result.has_value()) {
        return HttpResult::error(
            result.error().code() >= 400 ? result.error().code() : 500,
            result.error().message()
        );
    }

    glz::json_t response;
    response["ok"] = true;
    response["result"] = glz::json_t{
        {"taskId", task_id},
        {"status", "cancelled"}
    };

    return HttpResult::ok(response);
}

auto HttpGateway::handle_list_agents(const HttpRequest& req, const token::TokenInfo& token)
    -> HttpResult {
    // Check permissions
    if (!check_permission(token, "operator.read")) {
        return HttpResult::error(403, "Insufficient permissions");
    }

    // TODO: Query from AgentTeam or registry
    glz::json_t result;
    result["ok"] = true;
    result["result"] = glz::json_t{
        {"agents", glz::json_t::array()}
    };

    return HttpResult::ok(result);
}

auto HttpGateway::handle_get_agent(const HttpRequest& req, const token::TokenInfo& token)
    -> HttpResult {
    // Check permissions
    if (!check_permission(token, "operator.read")) {
        return HttpResult::error(403, "Insufficient permissions");
    }

    // Extract agent_id from path
    auto path = req.path;
    auto base_pos = path.find(config_.base_path + "/agents/");
    if (base_pos == std::string::npos) {
        return HttpResult::error(400, "Invalid agent ID");
    }

    std::string agent_id = path.substr(base_pos + config_.base_path.size() + 9);  // "/agents/"

    // TODO: Actually query Agent
    glz::json_t result;
    result["ok"] = false;
    result["error"] = glz::json_t{
        {"code", -32601},
        {"message", std::format("Agent not found: {}", agent_id)}
    };

    return HttpResult::ok(result);
}

auto HttpGateway::handle_register_agent(const HttpRequest& req, const token::TokenInfo& token)
    -> HttpResult {
    // Check permissions
    if (!check_permission(token, "operator.write")) {
        return HttpResult::error(403, "Insufficient permissions");
    }

    // Parse request body
    auto body_opt = parse_json_body(req);
    if (!body_opt.has_value()) {
        return HttpResult::error(400, "Invalid JSON body");
    }

    auto& body = body_opt.value();

    // Validate required fields
    if (!body.contains("agentId")) {
        return HttpResult::error(400, "Missing required field: agentId",
            glz::json_t{{"field", "agentId"}, {"reason", "required"}});
    }

    std::string agent_id = body["agentId"];

    // TODO: Actually register Agent
    glz::json_t result;
    result["ok"] = true;
    result["result"] = glz::json_t{
        {"agentId", agent_id},
        {"status", "registered"}
    };

    return HttpResult::created(result);
}

// ==================== Helper Methods ====================

auto HttpGateway::send_json(HttpResponse& res, const HttpResult& result) -> void {
    res.set_status(result.status_code);
    res.set_content_type(result.content_type);

    std::string body_str;
    glz::write_json(result.body, body_str);
    res.set_content(body_str);
}

auto HttpGateway::send_error(HttpResponse& res, int code, std::string_view message,
                                const glz::json_t& details) -> void {
    res.set_status(code);

    glz::json_t body;
    body["ok"] = false;
    body["error"] = glz::json_t{
        {"code", code},
        {"message", message}
    };

    if (!details.is_null() && details.size() > 0) {
        body["error"]["details"] = details;
    }

    std::string body_str;
    glz::write_json(body, body_str);
    res.set_content(body_str, "application/json");
}

auto HttpGateway::parse_json_body(const HttpRequest& req) const
    -> std::optional<glz::json_t> {
    try {
        glz::json_t json;
        auto err = glz::read_json(json, req.body);
        if (err) {
            return std::nullopt;
        }
        return json;
    } catch (...) {
        return std::nullopt;
    }
}

auto HttpGateway::get_client_ip(const HttpRequest& req) const -> std::string {
    // Try to get real IP (considering proxies)
    auto forwarded_for = req.get_header_value("X-Forwarded-For");
    if (!forwarded_for.empty()) {
        // X-Forwarded-For may contain multiple IPs, take the first one
        auto comma_pos = forwarded_for.find(',');
        if (comma_pos != std::string::npos) {
            return forwarded_for.substr(0, comma_pos);
        }
        return forwarded_for;
    }

    auto real_ip = req.get_header_value("X-Real-IP");
    if (!real_ip.empty()) {
        return real_ip;
    }

    return req.remote_addr;
}

auto HttpGateway::check_permission(const token::TokenInfo& token,
                                   std::string_view required_scope) const -> bool {
    // Check if token's scopes contain the required permission
    for (const auto& scope : token.scopes) {
        if (scope == required_scope) {
            return true;
        }

        // Support wildcards
        if (scope.size() > required_scope.size()) {
            auto wildcard_pos = scope.find('*');
            if (wildcard_pos != std::string::npos) {
                auto prefix = scope.substr(0, wildcard_pos);
                if (required_scope.substr(0, prefix.size()) == prefix) {
                    return true;
                }
            }
        }
    }

    return false;
}

// ==================== OpenAI Compatible API Implementation ====================

auto HttpGateway::handle_chat_completions(const HttpRequest& req, const token::TokenInfo& token)
    -> HttpResult
{
    // Parse request body
    auto body_opt = parse_json_body(req);
    if (!body_opt.has_value()) {
        return HttpResult::error(400, "Invalid JSON body");
    }

    auto& body = body_opt.value();

    // Validate required fields
    if (!body.contains("model") || !body["model"].is_string()) {
        return HttpResult::error(400, "Missing or invalid required field: model",
            glz::json_t{{"field", "model"}, {"reason", "required string"}});
    }

    if (!body.contains("messages") || !body["messages"].is_array()) {
        return HttpResult::error(400, "Missing or invalid required field: messages",
            glz::json_t{{"field", "messages"}, {"reason", "required array"}});
    }

    // Extract agent ID
    auto agent_id = extract_agent_id(req, body);

    // Extract parameters
    std::vector<glz::json_t> messages = body["messages"];
    double temperature = body.value("temperature", 1.0);
    double top_p = body.value("top_p", 1.0);
    int max_tokens = body.value("max_tokens", 1000);
    bool stream = body.value("stream", false);

    // Check permissions
    if (!check_permission(token, "task.execute")) {
        return HttpResult::error(403, "Insufficient permissions: task.execute required");
    }

    // Convert message format (OpenAI -> MoltCat)
    auto moltcat_params = convert_openai_messages_to_moltcat(messages);

    // Add other parameters
    if (body.contains("temperature")) {
        moltcat_params["temperature"] = temperature;
    }
    if (body.contains("top_p")) {
        moltcat_params["top_p"] = top_p;
    }
    if (body.contains("max_tokens")) {
        moltcat_params["max_tokens"] = max_tokens;
    }

    // Generate completion ID
    auto completion_id = "chatcmpl-" + generate_random_string(12);

    // Check if streaming response
    if (stream) {
        // Streaming response: set SSE headers
        res.set_content_provider(
            httplib::ContentProvider{
                [&](size_t offset, size_t length, httplib::DataSink& sink) {
                    // In actual implementation, this should connect to agent and stream results back
                    // Current simplified implementation: return complete JSON response
                    auto content = "data: " + build_openai_response(
                        completion_id, agent_id, "Streaming response (not yet fully implemented)",
                        "stop", 0, 1
                    ).dump() + "\n\ndata: [DONE]\n";
                    sink.write(content.data(), content.size());
                    return true;
                }
            }
        );
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");

        // Return 200 status (but don't close connection)
        return HttpResult{200, glz::json_t{}};
    }

    // Non-streaming response: create task
    auto task_params = glz::json_t{
        {"type", "llm.generate"},
        {"params", moltcat_params}
    };

    // Use user field as session key (if provided)
    std::string session_key;
    if (body.contains("user") && body["user"].is_string()) {
        std::string user_id = body["user"].get<std::string>();
        session_key = derive_session_key(user_id);
    }

    // Create task
    auto task_result = task_manager_->create_task(
        "llm.generate",
        task_params,
        token.device_id,
        session_key
    );

    if (!task_result.has_value()) {
        return HttpResult::error(
            task_result.error().code() >= 400 ? task_result.error().code() : 500,
            task_result.error().message()
        );
    }

    auto task_id = task_result.value();

    // TODO: Wait for task completion and get result
    // Current simplified implementation: directly return simulated response
    std::string response_content = "This is a placeholder response from agent: " + agent_id;

    // Build OpenAI format response
    glz::json_t response = build_openai_response(
        completion_id,
        agent_id,
        response_content,
        "stop",
        messages.size() * 5,  // Estimate prompt tokens
        response_content.size() / 4  // Estimate completion tokens
    );

    glz::json_t result;
    result["ok"] = true;
    result["result"] = response;

    return HttpResult::ok(result);
}

// ==================== OpenAI API Helper Methods ====================

auto HttpGateway::extract_agent_id(const HttpRequest& req, const glz::json_t& body) const
    -> std::string
{
    // 1. Check x-moltcat-agent-id header
    auto agent_header = req.get_header_value("x-moltcat-agent-id");
    if (!agent_header.empty()) {
        return agent_header;
    }

    // 2. Extract from model field
    if (body.contains("model") && body["model"].is_string()) {
        std::string model = body["model"].get<std::string>();

        // Check format: moltcat:<agentId> or agent:<agentId>
        if (model.substr(0, 9) == "moltcat:") {
            return model.substr(9);  // Remove "moltcat:" prefix
        } else if (model.substr(0, 6) == "agent:") {
            return model.substr(6);  // Remove "agent:" prefix
        }

        // If "openclaw" or others, use default agent
        if (model == "openclaw" || model == "openclaw:") {
            return config_.default_agent;
        }
    }

    // 3. Use default agent
    return config_.default_agent;
}

auto HttpGateway::derive_session_key(const std::string& user_id) const
    -> std::string
{
    // Generate stable session key based on user_id
    // Simplified implementation: use user_id + fixed prefix
    return "session:" + user_id;
}

auto HttpGateway::convert_openai_messages_to_moltcat(const glz::json_t& openai_messages) const
    -> glz::json_t
{
    glz::json_t moltcat_messages = glz::json_t::array();
    glz::json_t system_prompt;
    glz::json_t user_content;

    // Separate system messages and user messages
    for (const auto& msg : openai_messages) {
        if (!msg.contains("role") || !msg["role"].is_string()) {
            continue;  // Skip invalid messages
        }

        std::string role = msg["role"].get<std::string>();

        if (role == "system") {
            if (msg.contains("content") && msg["content"].is_string()) {
                system_prompt = msg["content"];
            }
        } else if (role == "user") {
            if (msg.contains("content") && msg["content"].is_string()) {
                user_content = msg["content"];
            }
        } else if (role == "assistant") {
            // For conversation history, can be handled here
            // Current simplified implementation: only keep last user message
        }
    }

    // Build MoltCat format message
    if (!system_prompt.is_null()) {
        // Merge system prompt into user content
        if (!user_content.is_null()) {
            user_content = "System: " + system_prompt.get<std::string>() +
                           "\n\nUser: " + user_content.get<std::string>();
        } else {
            user_content = system_prompt;
        }
    }

    // Estimate token count (simplified implementation)
    size_t estimated_tokens = user_content.get<std::string>().length() / 4;

    return glz::json_t{
        {"prompt", user_content},
        {"estimated_tokens", estimated_tokens}
    };
}

auto HttpGateway::build_openai_response(
    const std::string& completion_id,
    const std::string& agent_id,
    const std::string& content,
    const std::string& finish_reason,
    uint64_t prompt_tokens,
    uint64_t completion_tokens
) const -> glz::json_t
{
    return glz::json_t{
        {"id", completion_id},
        {"object", "chat.completion"},
        {"created", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()},
        {"model", "moltcat:" + agent_id},
        {"choices", glz::json_t::array({
            glz::json_t{
                {"index", 0},
                {"message", glz::json_t{
                    {"role", "assistant"},
                    {"content", content}
                }},
                {"finish_reason", finish_reason}
            }
        })},
        {"usage", glz::json_t{
            {"prompt_tokens", prompt_tokens},
            {"completion_tokens", completion_tokens},
            {"total_tokens", prompt_tokens + completion_tokens}
        }}
    };
}

auto HttpGateway::send_sse_chunk(HttpResponse& res, const std::string& chunk) -> void
{
    // Send SSE format data chunk
    std::string sse_data = "data: " + chunk + "\n\n";
    res.set_content(sse_data);
}

auto HttpGateway::end_sse_stream(HttpResponse& res) -> void
{
    // Send SSE end marker
    res.set_content("data: [DONE]\n\n");
}

// Generate random string (for completion ID)
auto HttpGateway::generate_random_string(size_t length) const -> std::string
{
    static const char chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string result;
    result.reserve(length);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);

    for (size_t i = 0; i < length; ++i) {
        result += chars[dis(gen)];
    }

    return result;
}

} // namespace moltcat::gateway
