#pragma once

#include "a2a_types.hpp"
#include "a2a_jsonrpc.hpp"
#include <httplib.h>
#include <functional>
#include <memory>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace moltcat::protocol::http {

// ================================
// A2A REST Endpoint Configuration
// ================================

struct A2AHttpConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    std::string base_path = "";                        // Optional: Base path (e.g., "/a2a/v1")
    bool enable_cors = true;
    size_t max_request_size = 10 * 1024 * 1024;        // 10MB
};

// ================================
// A2A HTTP Endpoint Handler
// ================================

/**
 * A2A HTTP Gateway - A2A REST endpoint implementation
 *
 * Strictly follows A2A specification REST endpoint definitions:
 * - POST /message:send
 * - POST /message:stream (not implemented yet)
 * - GET  /tasks/{id}
 * - GET  /tasks
 * - POST /tasks/{id}:cancel
 * - GET  /tasks/{id}:subscribe (not implemented yet)
 * - GET  /extendedAgentCard
 */
class A2AHttpGateway {
public:
    // ========== Method handler types ==========

    using SendMessageHandler = std::function<a2a::Task(const a2a::Message&, const std::string&)>;
    using GetTaskHandler = std::function<std::optional<a2a::Task>(const std::string&, const std::string&)>;
    using ListTasksHandler = std::function<std::vector<a2a::Task>(const std::string&, int, int, const std::string&)>;
    using CancelTaskHandler = std::function<std::optional<a2a::Task>(const std::string&, const std::string&)>;
    using GetAgentCardHandler = std::function<glz::json_t(const std::string&)>;

    // ========== Construction and startup ==========

    explicit A2AHttpGateway(const A2AHttpConfig& config);
    ~A2AHttpGateway();

    // Disable copy and move
    A2AHttpGateway(const A2AHttpGateway&) = delete;
    A2AHttpGateway& operator=(const A2AHttpGateway&) = delete;

    /**
     * Start HTTP server
     */
    auto start() -> bool;

    /**
     * Stop HTTP server
     */
    auto stop() -> void;

    /**
     * Register method handlers
     */

    // Send message
    auto on_send_message(SendMessageHandler handler) -> void {
        send_message_handler_ = std::move(handler);
    }

    // Get task
    auto on_get_task(GetTaskHandler handler) -> void {
        get_task_handler_ = std::move(handler);
    }

    // List tasks
    auto on_list_tasks(ListTasksHandler handler) -> void {
        list_tasks_handler_ = std::move(handler);
    }

    // Cancel task
    auto on_cancel_task(CancelTaskHandler handler) -> void {
        cancel_task_handler_ = std::move(handler);
    }

    // Get Agent Card
    auto on_get_agent_card(GetAgentCardHandler handler) -> void {
        get_agent_card_handler_ = std::move(handler);
    }

private:
    // ========== Configuration and state ==========

    A2AHttpConfig config_;
    std::unique_ptr<httplib::Server> server_;
    std::shared_ptr<spdlog::logger> logger_;
    std::atomic<bool> running_{false};

    // ========== Method handlers ==========

    SendMessageHandler send_message_handler_;
    GetTaskHandler get_task_handler_;
    ListTasksHandler list_tasks_handler_;
    CancelTaskHandler cancel_task_handler_;
    GetAgentCardHandler get_agent_card_handler_;

    // ========== Route registration ==========

    /**
     * Register all A2A REST endpoints
     */
    auto register_routes() -> void;

    /**
     * Register CORS middleware (if enabled)
     */
    auto register_cors() -> void;

    // ========== Endpoint handler functions ==========

    /**
     * POST /message:send
     *
     * A2A specification: Send message to Agent
     *
     * Request body (JSON-RPC format):
     * {
     *   "jsonrpc": "2.0",
     *   "method": "a2a.send_message",
     *   "params": {
     *     "tenant": "optional-tenant-id",
     *     "message": { ... },
     *     "configuration": { ... }
     *   },
     *   "id": "request-id"
     * }
     *
     * Response body (JSON-RPC format):
     * {
     *   "jsonrpc": "2.0",
     *   "result": { "task": { ... } },
     *   "error": null,
     *   "id": "request-id"
     * }
     */
    auto handle_send_message(const httplib::Request& req, httplib::Response& res) -> void;

    /**
     * GET /tasks/{id}
     *
     * A2A specification: Get task details
     *
     * Query parameters:
     * - history_length: Number of history messages (optional, default 10)
     *
     * Response body (JSON):
     * {
     *   "id": "task-uuid",
     *   "context_id": "ctx-uuid",
     *   "status": { ... },
     *   "artifacts": [ ... ],
     *   "history": [ ... ]
     * }
     */
    auto handle_get_task(const httplib::Request& req, httplib::Response& res) -> void;

    /**
     * GET /tasks
     *
     * A2A specification: List tasks
     *
     * Query parameters:
     * - tenant: Tenant ID (optional)
     * - context_id: Context ID (optional)
     * - status: Task status (optional)
     * - page_size: Items per page (optional, default 50)
     * - page_token: Pagination token (optional)
     * - history_length: Number of history messages (optional)
     *
     * Response body (JSON):
     * {
     *   "tasks": [ ... ],
     *   "next_page_token": "...",
     *   "page_size": 50,
     *   "total_size": 100
     * }
     */
    auto handle_list_tasks(const httplib::Request& req, httplib::Response& res) -> void;

    /**
     * POST /tasks/{id}:cancel
     *
     * A2A specification: Cancel task
     *
     * Request body (optional):
     * {
     *   "metadata": { ... }
     * }
     *
     * Response body (JSON): Task object
     */
    auto handle_cancel_task(const httplib::Request& req, httplib::Response& res) -> void;

    /**
     * GET /extendedAgentCard
     *
     * A2A specification: Get Agent capability description
     *
     * Response body (JSON): AgentCard object
     */
    auto handle_get_agent_card(const httplib::Request& req, httplib::Response& res) -> void;

    // ========== Helper methods ==========

    /**
     * Extract path parameter
     */
    auto get_path_param(const httplib::Request& req, std::string_view name) const -> std::string;

    /**
     * Extract query parameter
     */
    auto get_query_param(const httplib::Request& req, std::string_view name, const std::string& default_value = "") const -> std::string;

    auto get_query_param_int(const httplib::Request& req, std::string_view name, int default_value = 0) const -> int;

    /**
     * Construct JSON-RPC error response
     */
    auto make_error_response(int code, std::string_view message, const std::string& id = "") const -> std::string;

    /**
     * Set CORS response headers
     */
    auto set_cors_headers(httplib::Response& res) const -> void;
};

// ================================
// Implementation (inline)
// ================================

inline A2AHttpGateway::A2AHttpGateway(const A2AHttpConfig& config)
    : config_(config)
    , logger_(spdlog::get("moltcat"))
{
    server_ = std::make_unique<httplib::Server>(config.host, config.port);
    logger_->info("A2A HTTP Gateway created, listening on: {}:{}", config.host, config.port);
}

inline A2AHttpGateway::~A2AHttpGateway() {
    stop();
}

inline auto A2AHttpGateway::start() -> bool {
    if (running_.exchange(true)) {
        logger_->warn("A2A HTTP Gateway is already running");
        return true;
    }

    try {
        // Register routes
        register_routes();

        // Register CORS (if enabled)
        if (config_.enable_cors) {
            register_cors();
        }

        logger_->info("A2A HTTP Gateway started, endpoints: http://{}:{}{}",
            config_.host, config_.port, config_.base_path);

        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to start A2A HTTP Gateway: {}", e.what());
        running_ = false;
        return false;
    }
}

inline auto A2AHttpGateway::stop() -> void {
    if (!running_.exchange(false)) {
        return;
    }

    logger_->info("A2A HTTP Gateway shutting down...");
    server_.reset();
    logger_->info("A2A HTTP Gateway shut down");
}

inline auto A2AHttpGateway::register_routes() -> void {
    const std::string base = config_.base_path.empty() ? "" : config_.base_path;

    // POST /message:send
    server_->Post(base + "/message:send", [this](const httplib::Request& req, httplib::Response& res) {
        handle_send_message(req, res);
    });

    // GET /tasks/{id}
    server_->Get(base + "/tasks/:id", [this](const httplib::Request& req, httplib::Response& res) {
        handle_get_task(req, res);
    });

    // GET /tasks
    server_->Get(base + "/tasks", [this](const httplib::Request& req, httplib::Response& res) {
        handle_list_tasks(req, res);
    });

    // POST /tasks/{id}:cancel
    server_->Post(base + "/tasks/:id/cancel", [this](const httplib::Request& req, httplib::Response& res) {
        handle_cancel_task(req, res);
    });

    // GET /extendedAgentCard
    server_->Get(base + "/extendedAgentCard", [this](const httplib::Request& req, httplib::Response& res) {
        handle_get_agent_card(req, res);
    });

    logger_->info("A2A REST endpoints registered");
}

inline auto A2AHttpGateway::register_cors() -> void {
    // Set CORS preflight response
    server_->Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.set_header("Access-Control-Max-Age", "86400");
        return;
    });

    // Global middleware: Add CORS headers to all responses
    server_->set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        return httplib::Server::HandlerResponse::Unhandled;
    });
}

inline auto A2AHttpGateway::set_cors_headers(httplib::Response& res) const -> void {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Credentials", "true");
}

// ========== Endpoint handler implementations ==========

inline auto A2AHttpGateway::handle_send_message(const httplib::Request& req, httplib::Response& res) -> void {
    try {
        // 1. Parse JSON-RPC request
        auto rpc_req_opt = jsonrpc::JsonRpcRequest::deserialize(req.body);
        if (!rpc_req_opt) {
            res.status = 400;
            res.set_content(make_error_response(-32700, "Invalid JSON"), "application/json");
            return;
        }

        auto& rpc_req = *rpc_req_opt;

        // 2. Validate method name
        if (rpc_req.method != "a2a.send_message") {
            res.status = 400;
            res.set_content(make_error_response(-32601, "Method not found", rpc_req.id), "application/json");
            return;
        }

        // 3. Extract parameters
        if (!rpc_req.params.is_object()) {
            res.status = 400;
            res.set_content(make_error_response(-32602, "Invalid params", rpc_req.id), "application/json");
            return;
        }

        auto tenant = rpc_req.params.value("tenant", std::string(""));
        auto message_obj = rpc_req.params.at("message");

        // Deserialize Message
        a2a::Message message;
        auto error = glz::read_json(message, message_obj);
        if (error) {
            res.status = 400;
            res.set_content(make_error_response(-32602, "Invalid message format", rpc_req.id), "application/json");
            return;
        }

        // 4. Call handler
        if (!send_message_handler_) {
            res.status = 503;
            res.set_content(make_error_response(-32603, "Handler not registered", rpc_req.id), "application/json");
            return;
        }

        auto task = send_message_handler_(message, tenant);

        // 5. Construct JSON-RPC response
        jsonrpc::JsonRpcResponse rpc_resp = jsonrpc::JsonRpcResponse::success(
            rpc_req.id,
            glz::json_t{{"task", task}}
        );

        res.set_content(rpc_resp.serialize(), "application/json");

    } catch (const std::exception& e) {
        logger_->error("handle_send_message exception: {}", e.what());
        res.status = 500;
        res.set_content(make_error_response(-32603, e.what()), "application/json");
    }
}

inline auto A2AHttpGateway::handle_get_task(const httplib::Request& req, httplib::Response& res) -> void {
    try {
        // 1. Extract path parameter
        auto task_id = get_path_param(req, "id");

        // 2. Extract query parameters
        auto tenant = get_query_param(req, "tenant");
        auto history_length = get_query_param_int(req, "history_length", 10);

        // 3. Call handler
        if (!get_task_handler_) {
            res.status = 503;
            res.set_content(make_error_response(-32603, "Handler not registered"), "application/json");
            return;
        }

        auto task_opt = get_task_handler_(task_id, tenant);

        if (!task_opt) {
            res.status = 404;
            res.set_content(make_error_response(4001, "Task not found"), "application/json");
            return;
        }

        // 4. Return task (apply history_length filter)
        auto task = *task_opt;
        if (history_length >= 0 && static_cast<size_t>(history_length) < task.history.size()) {
            // Keep last N messages
            size_t start = task.history.size() - history_length;
            task.history = std::vector<a2a::Message>(task.history.begin() + start, task.history.end());
        }

        res.set_content(task.serialize(), "application/json");

    } catch (const std::exception& e) {
        logger_->error("handle_get_task exception: {}", e.what());
        res.status = 500;
        res.set_content(make_error_response(-32603, e.what()), "application/json");
    }
}

inline auto A2AHttpGateway::handle_list_tasks(const httplib::Request& req, httplib::Response& res) -> void {
    try {
        // 1. Extract query parameters
        auto tenant = get_query_param(req, "tenant");
        auto context_id = get_query_param(req, "context_id");
        auto page_size = get_query_param_int(req, "page_size", 50);
        auto page_token = get_query_param(req, "page_token");

        // 2. Call handler
        if (!list_tasks_handler_) {
            res.status = 503;
            res.set_content(make_error_response(-32603, "Handler not registered"), "application/json");
            return;
        }

        auto tasks = list_tasks_handler_(tenant, page_size, 0, page_token);

        // 3. Construct response
        glz::json_t response{
            {"tasks", tasks},
            {"next_page_token", ""},
            {"page_size", page_size},
            {"total_size", tasks.size()}
        };

        std::string json;
        glz::write_json(response, json);
        res.set_content(json, "application/json");

    } catch (const std::exception& e) {
        logger_->error("handle_list_tasks exception: {}", e.what());
        res.status = 500;
        res.set_content(make_error_response(-32603, e.what()), "application/json");
    }
}

inline auto A2AHttpGateway::handle_cancel_task(const httplib::Request& req, httplib::Response& res) -> void {
    try {
        // 1. Extract path parameter
        auto task_id = get_path_param(req, "id");

        // 2. Extract tenant (optional)
        auto tenant = get_query_param(req, "tenant");

        // 3. Call handler
        if (!cancel_task_handler_) {
            res.status = 503;
            res.set_content(make_error_response(-32603, "Handler not registered"), "application/json");
            return;
        }

        auto task_opt = cancel_task_handler_(task_id, tenant);

        if (!task_opt) {
            res.status = 404;
            res.set_content(make_error_response(4001, "Task not found"), "application/json");
            return;
        }

        res.set_content(task_opt->serialize(), "application/json");

    } catch (const std::exception& e) {
        logger_->error("handle_cancel_task exception: {}", e.what());
        res.status = 500;
        res.set_content(make_error_response(-32603, e.what()), "application/json");
    }
}

inline auto A2AHttpGateway::handle_get_agent_card(const httplib::Request& req, httplib::Response& res) -> void {
    try {
        // 1. Extract tenant (optional)
        auto tenant = get_query_param(req, "tenant");

        // 2. Call handler
        if (!get_agent_card_handler_) {
            res.status = 503;
            res.set_content(make_error_response(-32603, "Handler not registered"), "application/json");
            return;
        }

        auto card = get_agent_card_handler_(tenant);

        // 3. Serialize and return
        std::string json;
        glz::write_json(card, json);
        res.set_content(json, "application/json");

    } catch (const std::exception& e) {
        logger_->error("handle_get_agent_card exception: {}", e.what());
        res.status = 500;
        res.set_content(make_error_response(-32603, e.what()), "application/json");
    }
}

// ========== Helper method implementations ==========

inline auto A2AHttpGateway::get_path_param(const httplib::Request& req, std::string_view name) const -> std::string {
    auto it = req.params.find(std::string(name));
    if (it != req.params.end()) {
        return it->second;
    }
    return "";
}

inline auto A2AHttpGateway::get_query_param(const httplib::Request& req, std::string_view name, const std::string& default_value) const -> std::string {
    auto it = req.params.find(std::string(name));
    if (it != req.params.end()) {
        return it->second;
    }
    return default_value;
}

inline auto A2AHttpGateway::get_query_param_int(const httplib::Request& req, std::string_view name, int default_value) const -> int {
    auto str = get_query_param(req, name);
    if (str.empty()) {
        return default_value;
    }
    try {
        return std::stoi(str);
    } catch (...) {
        return default_value;
    }
}

inline auto A2AHttpGateway::make_error_response(int code, std::string_view message, const std::string& id) const -> std::string {
    jsonrpc::JsonRpcError error{code, std::string(message), std::nullopt};
    auto resp = jsonrpc::JsonRpcResponse::error(id, error);
    return resp.serialize();
}

} // namespace moltcat::protocol::http
