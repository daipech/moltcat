#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <glaze/json.hpp>

namespace moltcat::protocol {

/**
 * @brief WebSocket message frame type
 */
enum class FrameType : std::string_view {
    EVENT = "event",   // Server push event
    REQ = "req",       // Client request
    RES = "res",       // Server response
};

/**
 * @brief Parse message frame type
 */
[[nodiscard]] inline auto parse_frame_type(std::string_view type)
    -> std::optional<FrameType> {
    if (type == "event") return FrameType::EVENT;
    if (type == "req") return FrameType::REQ;
    if (type == "res") return FrameType::RES;
    return std::nullopt;
}

/**
 * @brief Get frame type string
 */
[[nodiscard]] inline auto frame_type_to_string(FrameType type) -> std::string_view {
    switch (type) {
        case FrameType::EVENT: return "event";
        case FrameType::REQ: return "req";
        case FrameType::RES: return "res";
    }
    return "";
}

// ==================== Event Frame ====================

/**
 * @brief WebSocket Event frame
 *
 * Server → Client event push
 */
struct WsEventFrame {
    std::string event;           // Event name (e.g., connect.challenge, tick)
    glz::json_t payload;        // Event payload
    std::optional<uint64_t> seq;         // Sequence number (optional)
    std::optional<uint64_t> state_version;  // State version (optional)

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] auto to_json() const -> std::string {
        glz::json_t json;
        json["type"] = "event";
        json["event"] = event;
        json["payload"] = payload;

        if (seq.has_value()) {
            json["seq"] = seq.value();
        }
        if (state_version.has_value()) {
            json["stateVersion"] = state_version.value();
        }

        std::string result;
        glz::write_json(json, result);
        return result;
    }

    /**
     * @brief Parse from JSON
     */
    static auto from_json(std::string_view json_str) -> std::optional<WsEventFrame> {
        try {
            glz::json_t json;
            auto err = glz::read_json(json, json_str);
            if (err) {
                return std::nullopt;
            }

            WsEventFrame frame;

            // Parse type
            auto type_it = json.find("type");
            if (type_it == json.end() || type_it->get<std::string>() != "event") {
                return std::nullopt;
            }

            // Parse event
            auto event_it = json.find("event");
            if (event_it != json.end()) {
                if (auto* s = event_it->get_if<std::string>()) {
                    frame.event = *s;
                }
            }

            // Parse payload
            auto payload_it = json.find("payload");
            if (payload_it != json.end()) {
                frame.payload = *payload_it;
            }

            // Parse optional fields
            auto seq_it = json.find("seq");
            if (seq_it != json.end()) {
                if (auto* n = seq_it->get_if<uint64_t>()) {
                    frame.seq = *n;
                }
            }

            auto state_it = json.find("stateVersion");
            if (state_it != json.end()) {
                if (auto* n = state_it->get_if<uint64_t>()) {
                    frame.state_version = *n;
                }
            }

            return frame;
        } catch (...) {
            return std::nullopt;
        }
    }
};

// ==================== Request Frame ====================

/**
 * @brief WebSocket Request frame
 *
 * Client → Server request
 */
struct WsRequestFrame {
    std::string id;               // Request ID (unique)
    std::string method;           // Method name
    glz::json_t params;           // Parameters

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] auto to_json() const -> std::string {
        glz::json_t json;
        json["type"] = "req";
        json["id"] = id;
        json["method"] = method;
        json["params"] = params;

        std::string result;
        glz::write_json(json, result);
        return result;
    }

    /**
     * @brief Parse from JSON
     */
    static auto from_json(std::string_view json_str) -> std::optional<WsRequestFrame> {
        try {
            glz::json_t json;
            auto err = glz::read_json(json, json_str);
            if (err) {
                return std::nullopt;
            }

            WsRequestFrame frame;

            // Parse type
            auto type_it = json.find("type");
            if (type_it == json.end() || type_it->get<std::string>() != "req") {
                return std::nullopt;
            }

            // Parse id
            auto id_it = json.find("id");
            if (id_it != json.end()) {
                if (auto* s = id_it->get_if<std::string>()) {
                    frame.id = *s;
                }
            }

            // Parse method
            auto method_it = json.find("method");
            if (method_it != json.end()) {
                if (auto* s = method_it->get_if<std::string>()) {
                    frame.method = *s;
                }
            }

            // Parse params
            auto params_it = json.find("params");
            if (params_it != json.end()) {
                frame.params = *params_it;
            }

            return frame;
        } catch (...) {
            return std::nullopt;
        }
    }
};

// ==================== Response Frame ====================

/**
 * @brief WebSocket Response frame
 *
 * Server → Client response
 */
struct WsResponseFrame {
    std::string id;               // Request ID (corresponds to request)
    bool ok;                      // Success flag
    std::optional<glz::json_t> payload;  // Payload on success
    std::optional<WsError> error;       // Error on failure

    /**
     * @brief Error information
     */
    struct WsError {
        int code;                  // Error code
        std::string message;        // Error message
        std::optional<glz::json_t> details;  // Detailed information
    };

    /**
     * @brief Create success response
     */
    static auto success(std::string_view id, const glz::json_t& payload = {})
        -> WsResponseFrame {
        WsResponseFrame frame;
        frame.id = id;
        frame.ok = true;
        frame.payload = payload;
        return frame;
    }

    /**
     * @brief Create error response
     */
    static auto error(std::string_view id, int code, std::string_view message,
                      const glz::json_t& details = {}) -> WsResponseFrame {
        WsResponseFrame frame;
        frame.id = id;
        frame.ok = false;

        WsError err;
        err.code = code;
        err.message = message;
        if (!details.is_null() && details.size() > 0) {
            err.details = details;
        }

        frame.error = err;
        return frame;
    }

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] auto to_json() const -> std::string {
        glz::json_t json;
        json["type"] = "res";
        json["id"] = id;
        json["ok"] = ok;

        if (ok && payload.has_value()) {
            json["payload"] = payload.value();
        }

        if (!ok && error.has_value()) {
            glz::json_t err_json;
            err_json["code"] = error.value().code;
            err_json["message"] = error.value().message;
            if (error.value().details.has_value()) {
                err_json["details"] = error.value().details.value();
            }
            json["error"] = err_json;
        }

        std::string result;
        glz::write_json(json, result);
        return result;
    }

    /**
     * @brief Parse from JSON
     */
    static auto from_json(std::string_view json_str) -> std::optional<WsResponseFrame> {
        try {
            glz::json_t json;
            auto err = glz::read_json(json, json_str);
            if (err) {
                return std::nullopt;
            }

            WsResponseFrame frame;

            // Parse type
            auto type_it = json.find("type");
            if (type_it == json.end() || type_it->get<std::string>() != "res") {
                return std::nullopt;
            }

            // Parse id
            auto id_it = json.find("id");
            if (id_it != json.end()) {
                if (auto* s = id_it->get_if<std::string>()) {
                    frame.id = *s;
                }
            }

            // Parse ok
            auto ok_it = json.find("ok");
            if (ok_it != json.end()) {
                if (auto* b = ok_it->get_if<bool>()) {
                    frame.ok = *b;
                }
            }

            // Parse payload or error
            if (frame.ok) {
                auto payload_it = json.find("payload");
                if (payload_it != json.end()) {
                    frame.payload = *payload_it;
                }
            } else {
                auto error_it = json.find("error");
                if (error_it != json.end()) {
                    WsError err;
                    auto err_obj = *error_it;

                    auto code_it = err_obj.find("code");
                    if (code_it != err_obj.end()) {
                        if (auto* n = code_it->get_if<int>()) {
                            err.code = *n;
                        }
                    }

                    auto msg_it = err_obj.find("message");
                    if (msg_it != err_obj.end()) {
                        if (auto* s = msg_it->get_if<std::string>()) {
                            err.message = *s;
                        }
                    }

                    auto details_it = err_obj.find("details");
                    if (details_it != err_obj.end()) {
                        err.details = *details_it;
                    }

                    frame.error = err;
                }
            }

            return frame;
        } catch (...) {
            return std::nullopt;
        }
    }
};

// ==================== Error Code Definitions ====================

/**
 * @brief WebSocket error codes
 */
enum class WsErrorCode {
    // JSON-RPC standard error codes
    PARSE_ERROR = -32700,         // JSON parse error
    INVALID_REQUEST = -32600,     // Invalid request
    METHOD_NOT_FOUND = -32601,    // Method not found
    INVALID_PARAMS = -32602,      // Invalid parameters
    INTERNAL_ERROR = -32603,      // Internal error

    // Authentication errors
    UNAUTHORIZED = 1001,          // Unauthorized
    FORBIDDEN = 1002,             // Permission denied
    DEVICE_NOT_FOUND = 1003,      // Device not found
    TOKEN_EXPIRED = 1004,         // Token expired
    TOKEN_REVOKED = 1005,         // Token revoked

    // Device authentication errors
    INVALID_SIGNATURE = 1101,     // Invalid signature
    CHALLENGE_EXPIRED = 1102,     // Challenge expired
    DEVICE_NOT_REGISTERED = 1103, // Device not registered

    // Protocol errors
    UNSUPPORTED_PROTOCOL = 1200,  // Unsupported protocol version
};

/**
 * @brief Get error code information
 */
[[nodiscard]] inline auto get_error_info(WsErrorCode code)
    -> std::pair<int, std::string> {
    return {static_cast<int>(code), ""};
}

} // namespace moltcat::protocol
