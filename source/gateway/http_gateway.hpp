#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <optional>

#include "connection_info.hpp"
#include "task_manager.hpp"
#include "auth/auth_manager.hpp"
#include "auth/permission_checker.hpp"
#include "../messaging/message_bus.hpp"
#include "../utils/error.hpp"

// httplib forward declarations
namespace httplib {
    class Server;
    class Request;
    class Response;
    struct ContentReader;
}

using HttpRequest = httplib::Request;
using HttpResponse = httplib::Response;
using ContentReader = httplib::ContentReader;

namespace moltcat::gateway {

/**
 * @brief HTTP Gateway configuration
 */
struct HttpGatewayConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    std::string base_path = "/api/v1";

    // Authentication configuration
    bool require_auth = true;

    // Rate limiting configuration (reserved)
    bool enable_rate_limit = false;
    size_t max_requests_per_minute = 60;

    // Idempotency configuration
    bool enable_idempotency = true;
    size_t idempotency_ttl_seconds = 3600;  // 1 hour

    // CORS configuration (reserved)
    bool enable_cors = false;
    std::string cors_origin = "*";

    // OpenAI compatible API configuration
    bool enable_openai_api = false;          // Whether to enable OpenAI compatible API
    std::string openai_base_path = "/v1";   // OpenAI API base path
    std::string default_agent = "main";       // Default agent
};

/**
 * @brief HTTP handler return type
 */
struct HttpResult {
    int status_code = 200;
    glz::json_t body;
    std::string content_type = "application/json";

    static auto ok(const glz::json_t& data = {}) -> HttpResult {
        return HttpResult{200, data};
    }

    static auto created(const glz::json_t& data = {}) -> HttpResult {
        return HttpResult{201, data};
    }

    static auto accepted(const glz::json_t& data = {}) -> HttpResult {
        return HttpResult{202, data};
    }

    static auto error(int code, std::string_view message) -> HttpResult {
        glz::json_t body;
        body["ok"] = false;
        body["error"] = glz::json_t{
            {"code", code},
            {"message", message}
        };
        return HttpResult{code >= 400 ? code : 500, body};
    }

    static auto error(int code, std::string_view message, const glz::json_t& details) -> HttpResult {
        glz::json_t body;
        body["ok"] = false;
        body["error"] = glz::json_t{
            {"code", code},
            {"message", message},
            {"details", details}
        };
        return HttpResult{code >= 400 ? code : 500, body};
    }
};

/**
 * @brief HTTP handler type
 */
using HttpHandler = std::function<HttpResult(
    const HttpRequest& req,          // HTTP request
    const token::TokenInfo& token     // Token information (authenticated)
)>;

/**
 * @brief HTTP Gateway
 *
 * Provides REST API as a simplified version of WebSocket protocol
 */
class HttpGateway {
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
    HttpGateway(
        const HttpGatewayConfig& config,
        std::shared_ptr<auth::AuthManager> auth_manager,
        std::shared_ptr<auth::PermissionChecker> permission_checker,
        std::shared_ptr<TaskManager> task_manager,
        messaging::MessageBusClient* message_bus_client = nullptr
    );

    /**
     * @brief Destructor
     */
    ~HttpGateway();

    // Disable copy
    HttpGateway(const HttpGateway&) = delete;
    HttpGateway& operator=(const HttpGateway&) = delete;

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
     * @brief Check if running
     *
     * @return bool Whether running
     */
    [[nodiscard]] auto is_running() const -> bool;

private:
    HttpGatewayConfig config_;
    std::shared_ptr<auth::AuthManager> auth_manager_;
    std::shared_ptr<auth::PermissionChecker> permission_checker_;
    std::shared_ptr<TaskManager> task_manager_;
    messaging::MessageBusClient* message_bus_client_{nullptr};  // Non-owning, externally managed

    std::unique_ptr<httplib::Server> server_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;

    // Idempotency key storage
    struct IdempotencyRecord {
        std::string response_body;
        int status_code;
        std::string content_type;
        std::chrono::system_clock::time_point created_at;
    };
    std::unordered_map<std::string, IdempotencyRecord> idempotency_cache_;
    std::mutex idempotency_mutex_;

    // Rate limiting storage (reserved)
    std::unordered_map<std::string, std::vector<std::chrono::system_clock::time_point>> rate_limit_cache_;
    std::mutex rate_limit_mutex_;

    // ==================== Route Setup ====================

    /**
     * @brief Setup all routes
     */
    auto setup_routes() -> void;

    // ==================== Authentication Middleware ====================

    /**
     * @brief Authentication middleware
     *
     * @param req Request
     * @param res Response
     * @return bool Whether authentication passed
     */
    auto auth_middleware(const HttpRequest& req, HttpResponse& res) -> bool;

    /**
     * @brief Extract Bearer Token from request
     *
     * @param req Request
     * @return std::optional<std::string> Token string
     */
    [[nodiscard]] auto extract_bearer_token(const HttpRequest& req) const
        -> std::optional<std::string>;

    // ==================== Idempotency Middleware ====================

    /**
     * @brief Idempotency middleware
     *
     * @param req Request
     * @param res Response
     * @param handler Handler
     */
    auto idempotency_middleware(
        const HttpRequest& req,
        HttpResponse& res,
        std::function<HttpResult()> handler
    ) -> void;

    /**
     * @brief Clean up expired idempotency keys
     */
    auto cleanup_idempotency_keys() -> void;

    // ==================== Rate Limiting Middleware ====================

    /**
     * @brief Rate limiting middleware (reserved)
     *
     * @param req Request
     * @param res Response
     * @return bool Whether rate limit check passed
     */
    auto rate_limit_middleware(const HttpRequest& req, HttpResponse& res) -> bool;

    // ==================== API Endpoint Handlers ====================

    // System status
    auto handle_system_status(const HttpRequest& req, const token::TokenInfo& token)
        -> HttpResult;

    auto handle_system_presence(const HttpRequest& req, const token::TokenInfo& token)
        -> HttpResult;

    // Task management
    auto handle_create_task(const HttpRequest& req, const token::TokenInfo& token)
        -> HttpResult;

    auto handle_get_task(const HttpRequest& req, const token::TokenInfo& token)
        -> HttpResult;

    auto handle_list_tasks(const HttpRequest& req, const token::TokenInfo& token)
        -> HttpResult;

    auto handle_cancel_task(const HttpRequest& req, const token::TokenInfo& token)
        -> HttpResult;

    // Agent management
    auto handle_list_agents(const HttpRequest& req, const token::TokenInfo& token)
        -> HttpResult;

    auto handle_get_agent(const HttpRequest& req, const token::TokenInfo& token)
        -> HttpResult;

    auto handle_register_agent(const HttpRequest& req, const token::TokenInfo& token)
        -> HttpResult;

    // ==================== OpenAI Compatible API Endpoints ====================

    auto handle_chat_completions(const HttpRequest& req, const token::TokenInfo& token)
        -> HttpResult;

    // ==================== Helper Methods ====================

    /**
     * @brief Send JSON response
     */
    auto send_json(HttpResponse& res, const HttpResult& result) -> void;

    /**
     * @brief Send error response
     */
    auto send_error(HttpResponse& res, int code, std::string_view message,
                    const glz::json_t& details = {}) -> void;

    /**
     * @brief Parse JSON request body
     */
    [[nodiscard]] auto parse_json_body(const HttpRequest& req) const
        -> std::optional<glz::json_t>;

    /**
     * @brief Get client IP
     */
    [[nodiscard]] auto get_client_ip(const HttpRequest& req) const -> std::string;

    /**
     * @brief Check permission
     */
    [[nodiscard]] auto check_permission(
        const token::TokenInfo& token,
        std::string_view required_scope
    ) const -> bool;

    // ==================== OpenAI API Helper Methods ====================

    /**
     * @brief Extract agent ID from request
     *
     * Priority: x-moltcat-agent-id header > model field > default agent
     */
    [[nodiscard]] auto extract_agent_id(const HttpRequest& req, const glz::json_t& body) const
        -> std::string;

    /**
     * @brief Generate session key
     *
     * Generate stable session key from user field
     */
    [[nodiscard]] auto derive_session_key(const std::string& user_id) const -> std::string;

    /**
     * @brief Convert message format (OpenAI -> MoltCat)
     */
    [[nodiscard]] auto convert_openai_messages_to_moltcat(const glz::json_t& openai_messages) const
        -> glz::json_t;

    /**
     * @brief Build OpenAI-style response
     */
    [[nodiscard]] auto build_openai_response(
        const std::string& completion_id,
        const std::string& agent_id,
        const std::string& content,
        const std::string& finish_reason,
        uint64_t prompt_tokens,
        uint64_t completion_tokens
    ) const -> glz::json_t;

    /**
     * @brief Send SSE streaming response
     */
    auto send_sse_chunk(HttpResponse& res, const std::string& chunk) -> void;

    /**
     * @brief End SSE stream
     */
    auto end_sse_stream(HttpResponse& res) -> void;
};

} // namespace moltcat::gateway
