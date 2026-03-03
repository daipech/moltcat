#pragma once

#include "a2a_types.hpp"
#include <glaze/json.hpp>
#include <functional>
#include <unordered_map>
#include <string>
#include <optional>
#include <stdexcept>

namespace moltcat::protocol::jsonrpc {

// ================================
// JSON-RPC 2.0 Standard Error Codes
// ================================

enum class JsonRpcErrorCode : int {
    // Standard error codes (JSON-RPC 2.0 specification)
    PARSE_ERROR = -32700,              // JSON parse error
    INVALID_REQUEST = -32600,          // Invalid JSON-RPC request
    METHOD_NOT_FOUND = -32601,         // Method not found
    INVALID_PARAMS = -32602,           // Invalid parameters
    INTERNAL_ERROR = -32603,           // Internal server error

    // A2A custom error codes
    TASK_NOT_FOUND = 4001,             // Task not found
    TASK_ALREADY_COMPLETED = 4002,     // Task already completed
    CONTEXT_NOT_FOUND = 4003,          // Context not found
    AUTHENTICATION_REQUIRED = 4004,    // Authentication required
    UNSUPPORTED_OPERATION = 4005       // Unsupported operation
};

// ================================
// JSON-RPC 2.0 Request/Response Structures
// ================================

/**
 * JSON-RPC 2.0 Request
 *
 * Specification: https://www.jsonrpc.org/specification
 */
struct JsonRpcRequest {
    std::string jsonrpc = "2.0";                     // Required: JSON-RPC version
    std::string method;                              // Required: Method name
    glz::json_t params;                              // Optional: Parameters (object or array)
    std::string id;                                  // Required: Request ID (for correlating response)

    /**
     * Validate request format
     */
    auto validate() const -> std::optional<std::string> {
        if (jsonrpc != "2.0") {
            return "JSON-RPC version must be 2.0";
        }
        if (method.empty()) {
            return "Method name cannot be empty";
        }
        if (id.empty()) {
            return "Request ID cannot be empty";
        }
        return std::nullopt;  // Validation passed
    }

    /**
     * Serialize to JSON
     */
    auto serialize() const -> std::string {
        std::string json;
        auto error = glz::write_json(*this, json);
        if (error) {
            throw std::runtime_error("JSON-RPC request serialization failed: " + std::string(error));
        }
        return json;
    }

    /**
     * Deserialize from JSON
     */
    static auto deserialize(const std::string& json) -> std::optional<JsonRpcRequest> {
        JsonRpcRequest req;
        auto error = glz::read_json(req, json);
        if (error) {
            return std::nullopt;
        }
        return req;
    }
};

// Glaze 序列化支持
template <>
struct glz::meta<JsonRpcRequest> {
    using T = JsonRpcRequest;
    static constexpr auto value = glz::object(
        "jsonrpc", &T::jsonrpc,
        "method", &T::method,
        "params", &T::params,
        "id", &T::id
    );
};

/**
 * JSON-RPC 2.0 Error object
 */
struct JsonRpcError {
    int code;                                        // Required: Error code
    std::string message;                             // Required: Error description
    std::optional<glz::json_t> data;                // Optional: Additional error data

    auto serialize() const -> glz::json_t {
        glz::json_t error_obj{
            {"code", code},
            {"message", message}
        };
        if (data) {
            error_obj["data"] = *data;
        }
        return error_obj;
    }

    // Standard error factory methods
    static auto parse_error() -> JsonRpcError {
        return {static_cast<int>(JsonRpcErrorCode::PARSE_ERROR), "Parse error", std::nullopt};
    }

    static auto invalid_request(std::string_view detail = "") -> JsonRpcError {
        glz::json_t data;
        if (!detail.empty()) {
            data = glz::json_t{{"detail", std::string(detail)}};
        }
        return {static_cast<int>(JsonRpcErrorCode::INVALID_REQUEST), "Invalid request", data};
    }

    static auto method_not_found(std::string_view method) -> JsonRpcError {
        glz::json_t data{{"method", std::string(method)}};
        return {static_cast<int>(JsonRpcErrorCode::METHOD_NOT_FOUND), "Method not found", data};
    }

    static auto invalid_params(std::string_view detail = "") -> JsonRpcError {
        glz::json_t data;
        if (!detail.empty()) {
            data = glz::json_t{{"detail", std::string(detail)}};
        }
        return {static_cast<int>(JsonRpcErrorCode::INVALID_PARAMS), "Invalid parameters", data};
    }

    static auto internal_error(std::string_view detail = "") -> JsonRpcError {
        glz::json_t data;
        if (!detail.empty()) {
            data = glz::json_t{{"detail", std::string(detail)}};
        }
        return {static_cast<int>(JsonRpcErrorCode::INTERNAL_ERROR), "Internal error", data};
    }

    // A2A custom errors
    static auto task_not_found(std::string_view task_id) -> JsonRpcError {
        glz::json_t data{{"task_id", std::string(task_id)}};
        return {static_cast<int>(JsonRpcErrorCode::TASK_NOT_FOUND), "Task not found", data};
    }

    static auto authentication_required() -> JsonRpcError {
        return {static_cast<int>(JsonRpcErrorCode::AUTHENTICATION_REQUIRED), "Authentication required", std::nullopt};
    }
};

/**
 * JSON-RPC 2.0 Response
 */
struct JsonRpcResponse {
    std::string jsonrpc = "2.0";                     // Required: JSON-RPC version
    glz::json_t result;                              // Required on success
    std::optional<JsonRpcError> error;               // Required on error
    std::string id;                                  // Required: Request ID (corresponds to request)

    /**
     * Construct success response
     */
    static auto success(const std::string& id, const glz::json_t& result) -> JsonRpcResponse {
        return JsonRpcResponse{
            .result = result,
            .error = std::nullopt,
            .id = id
        };
    }

    /**
     * Construct error response
     */
    static auto error(const std::string& id, const JsonRpcError& error_obj) -> JsonRpcResponse {
        return JsonRpcResponse{
            .result = nullptr,
            .error = error_obj,
            .id = id
        };
    }

    /**
     * Check if error response
     */
    auto is_error() const -> bool {
        return error.has_value();
    }

    /**
     * Serialize to JSON
     */
    auto serialize() const -> std::string {
        std::string json;
        auto error = glz::write_json(*this, json);
        if (error) {
            throw std::runtime_error("JSON-RPC response serialization failed: " + std::string(error));
        }
        return json;
    }
};

// Glaze serialization support (requires custom handling of error field)
template <>
struct glz::meta<JsonRpcResponse> {
    using T = JsonRpcResponse;
    static constexpr auto value = glz::object(
        "jsonrpc", &T::jsonrpc,
        "result", &T::result,
        "error", &T::error,
        "id", &T::id
    );
};

// ================================
// JSON-RPC Method Handlers
// ================================

/**
 * Method handler function type
 *
 * @param params Method parameters (extracted from JSON-RPC request)
 * @return Method execution result (will be placed in JSON-RPC response's result field)
 */
using MethodHandler = std::function<glz::json_t(const glz::json_t& params)>;

/**
 * Async method handler function type
 *
 * @param params Method parameters
 * @return Future of method execution result
 */
using AsyncMethodHandler = std::function<std::future<glz::json_t>(const glz::json_t& params)>;

/**
 * JSON-RPC 2.0 Method Dispatcher
 *
 * Responsibilities:
 * 1. Register method handlers
 * 2. Receive JSON-RPC requests
 * 3. Dispatch to corresponding handlers
 * 4. Construct JSON-RPC responses
 */
class JsonRpcDispatcher {
public:
    JsonRpcDispatcher() = default;
    ~JsonRpcDispatcher() = default;

    // Disable copy and move
    JsonRpcDispatcher(const JsonRpcDispatcher&) = delete;
    JsonRpcDispatcher& operator=(const JsonRpcDispatcher&) = delete;

    /**
     * Register synchronous method handler
     *
     * @param method_name Method name (e.g., "a2a.send_message")
     * @param handler Handler function
     */
    auto register_method(std::string_view method_name, MethodHandler handler) -> void {
        handlers_[std::string(method_name)] = std::move(handler);
    }

    /**
     * Register asynchronous method handler
     *
     * @param method_name Method name
     * @param handler Asynchronous handler function
     */
    auto register_async_method(std::string_view method_name, AsyncMethodHandler handler) -> void {
        async_handlers_[std::string(method_name)] = std::move(handler);
    }

    /**
     * Check if method is registered
     */
    auto has_method(std::string_view method_name) const -> bool {
        std::string name(method_name);
        return handlers_.find(name) != handlers_.end()
            || async_handlers_.find(name) != async_handlers_.end();
    }

    /**
     * Handle JSON-RPC request (synchronous)
     *
     * @param json_request JSON serialized request string
     * @return JSON serialized response string
     */
    auto handle_request(const std::string& json_request) -> std::string {
        try {
            // 1. Parse request
            auto req_opt = JsonRpcRequest::deserialize(json_request);
            if (!req_opt) {
                auto error_resp = JsonRpcResponse::error(
                    "",
                    JsonRpcError::parse_error()
                );
                return error_resp.serialize();
            }

            auto& req = *req_opt;

            // 2. Validate request
            auto validation_error = req.validate();
            if (validation_error) {
                auto error_resp = JsonRpcResponse::error(
                    req.id,
                    JsonRpcError::invalid_request(*validation_error)
                );
                return error_resp.serialize();
            }

            // 3. Dispatch to handler
            auto resp = dispatch(req);

            return resp.serialize();

        } catch (const std::exception& e) {
            // Unhandled exception
            auto error_resp = JsonRpcResponse::error(
                "",
                JsonRpcError::internal_error(e.what())
            );
            return error_resp.serialize();
        }
    }

    /**
     * Handle JSON-RPC request (return object)
     */
    auto handle_request_object(const JsonRpcRequest& req) -> JsonRpcResponse {
        try {
            // Validate request
            auto validation_error = req.validate();
            if (validation_error) {
                return JsonRpcResponse::error(
                    req.id,
                    JsonRpcError::invalid_request(*validation_error)
                );
            }

            // Dispatch to handler
            return dispatch(req);

        } catch (const std::exception& e) {
            return JsonRpcResponse::error(
                req.id,
                JsonRpcError::internal_error(e.what())
            );
        }
    }

private:
    /**
     * Dispatch request to corresponding handler
     */
    auto dispatch(const JsonRpcRequest& req) -> JsonRpcResponse {
        std::string method = req.method;

        // Find synchronous handler
        auto it = handlers_.find(method);
        if (it != handlers_.end()) {
            try {
                auto result = it->second(req.params);
                return JsonRpcResponse::success(req.id, result);
            } catch (const std::exception& e) {
                return JsonRpcResponse::error(
                    req.id,
                    JsonRpcError::internal_error(e.what())
                );
            }
        }

        // Find asynchronous handler (not implemented yet, interface reserved)
        auto async_it = async_handlers_.find(method);
        if (async_it != async_handlers_.end()) {
            // TODO: Implement async handling
            return JsonRpcResponse::error(
                req.id,
                JsonRpcError::internal_error("Async handling not implemented")
            );
        }

        // Method not found
        return JsonRpcResponse::error(
            req.id,
            JsonRpcError::method_not_found(method)
        );
    }

    // Synchronous method handler registry
    std::unordered_map<std::string, MethodHandler> handlers_;

    // Asynchronous method handler registry
    std::unordered_map<std::string, AsyncMethodHandler> async_handlers_;
};

// ================================
// Helper functions: Construct A2A JSON-RPC requests
// ================================

/**
 * Construct A2A SendMessage JSON-RPC request
 */
inline auto build_send_message_request(
    const a2a::Message& message,
    const std::string& id = ""
) -> JsonRpcRequest
{
    JsonRpcRequest req;
    req.method = "a2a.send_message";
    req.params = glz::json_t{
        {"tenant", ""},  // Optional tenant ID
        {"message", message},
        {"configuration", glz::json_t{
            {"accepted_output_modes", std::vector<std::string>{"text/plain"}},
            {"push_notification_config", nullptr},
            {"history_length", 10},
            {"blocking", false}
        }},
        {"metadata", glz::json_t{}}
    };
    req.id = id.empty() ? utils::UUID::generate_v4() : id;
    return req;
}

/**
 * Construct A2A GetTask JSON-RPC request
 */
inline auto build_get_task_request(
    std::string_view task_id,
    int history_length = 10,
    const std::string& id = ""
) -> JsonRpcRequest
{
    JsonRpcRequest req;
    req.method = "a2a.get_task";
    req.params = glz::json_t{
        {"tenant", ""},
        {"id", std::string(task_id)},
        {"history_length", history_length}
    };
    req.id = id.empty() ? utils::UUID::generate_v4() : id;
    return req;
}

/**
 * Construct A2A CancelTask JSON-RPC request
 */
inline auto build_cancel_task_request(
    std::string_view task_id,
    const std::string& id = ""
) -> JsonRpcRequest
{
    JsonRpcRequest req;
    req.method = "a2a.cancel_task";
    req.params = glz::json_t{
        {"tenant", ""},
        {"id", std::string(task_id)},
        {"metadata", glz::json_t{}}
    };
    req.id = id.empty() ? utils::UUID::generate_v4() : id;
    return req;
}

/**
 * Construct A2A ListTasks JSON-RPC request
 */
inline auto build_list_tasks_request(
    std::string_view context_id = "",
    int page_size = 50,
    const std::string& page_token = "",
    const std::string& id = ""
) -> JsonRpcRequest
{
    JsonRpcRequest req;
    req.method = "a2a.list_tasks";
    req.params = glz::json_t{
        {"tenant", ""},
        {"context_id", std::string(context_id)},
        {"page_size", page_size},
        {"page_token", std::string(page_token)}
    };
    req.id = id.empty() ? utils::UUID::generate_v4() : id;
    return req;
}

} // namespace moltcat::protocol::jsonrpc
