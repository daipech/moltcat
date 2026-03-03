#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <glaze/json.hpp>
#include "ws_frames.hpp"
#include "ws_methods.hpp"

namespace moltcat::protocol {

// ==================== Connect Method Schema ====================

/**
 * @brief connect request parameters
 */
struct ConnectParams {
    // Protocol version
    uint32_t min_protocol = 1;
    uint32_t max_protocol = 1;

    // Client information
    struct ClientInfo {
        std::string id;              // Client ID (e.g., moltcat-cli)
        std::string version;         // Client version
        std::string platform;        // Platform (linux, windows, darwin)
        std::string mode;            // Mode (optional field for operator)
    };
    ClientInfo client;

    // Role and permissions
    std::string role;                       // operator | node
    std::vector<std::string> scopes;       // operator.read, operator.write...
    std::vector<std::string> caps;         // code-generation, code-review... (node)
    std::vector<std::string> commands;     // task.execute, task.cancel... (node)
    std::map<std::string, bool> permissions;  // Fine-grained permissions (node)

    // Authentication information
    struct AuthInfo {
        std::string token;  // Bearer Token or API Key
    };
    std::optional<AuthInfo> auth;

    // Device information
    struct DeviceInfo {
        std::string id;              // Device ID (derived from public key)
        std::string public_key;      // Base64 encoded public key
        std::string signature;       // Signature response
        uint64_t signed_at;          // Signature timestamp
        std::string nonce;           // Server-provided nonce
    };
    std::optional<DeviceInfo> device;

    // Other
    std::string locale = "en-US";    // Locale setting
    std::string user_agent;          // User-Agent

    /**
     * @brief Parse from JSON
     */
    static auto from_json(const glz::json_t& json) -> std::optional<ConnectParams> {
        try {
            ConnectParams params;

            // Parse min_protocol
            if (auto it = json.find("minProtocol"); it != json.end()) {
                if (auto* n = it->get_if<uint32_t>()) {
                    params.min_protocol = *n;
                }
            }

            // Parse max_protocol
            if (auto it = json.find("maxProtocol"); it != json.end()) {
                if (auto* n = it->get_if<uint32_t>()) {
                    params.max_protocol = *n;
                }
            }

            // Parse client
            if (auto it = json.find("client"); it != json.end()) {
                if (auto* obj = it->get_if<glz::json_t>()) {
                    auto& client_obj = *obj;

                    if (auto id_it = client_obj.find("id"); id_it != client_obj.end()) {
                        if (auto* s = id_it->get_if<std::string>()) {
                            params.client.id = *s;
                        }
                    }

                    if (auto ver_it = client_obj.find("version"); ver_it != client_obj.end()) {
                        if (auto* s = ver_it->get_if<std::string>()) {
                            params.client.version = *s;
                        }
                    }

                    if (auto plat_it = client_obj.find("platform"); plat_it != client_obj.end()) {
                        if (auto* s = plat_it->get_if<std::string>()) {
                            params.client.platform = *s;
                        }
                    }

                    if (auto mode_it = client_obj.find("mode"); mode_it != client_obj.end()) {
                        if (auto* s = mode_it->get_if<std::string>()) {
                            params.client.mode = *s;
                        }
                    }
                }
            }

            // Parse role
            if (auto it = json.find("role"); it != json.end()) {
                if (auto* s = it->get_if<std::string>()) {
                    params.role = *s;
                }
            }

            // Parse scopes
            if (auto it = json.find("scopes"); it != json.end()) {
                if (auto* arr = it->get_if<std::vector<std::string>>()) {
                    params.scopes = *arr;
                }
            }

            // Parse caps
            if (auto it = json.find("caps"); it != json.end()) {
                if (auto* arr = it->get_if<std::vector<std::string>>()) {
                    params.caps = *arr;
                }
            }

            // Parse commands
            if (auto it = json.find("commands"); it != json.end()) {
                if (auto* arr = it->get_if<std::vector<std::string>>()) {
                    params.commands = *arr;
                }
            }

            // Parse permissions
            if (auto it = json.find("permissions"); it != json.end()) {
                if (auto* obj = it->get_if<glz::json_t>()) {
                    for (const auto& [key, value] : *obj) {
                        if (auto* b = value.get_if<bool>()) {
                            params.permissions[key] = *b;
                        }
                    }
                }
            }

            // Parse auth
            if (auto it = json.find("auth"); it != json.end()) {
                if (auto* obj = it->get_if<glz::json_t>()) {
                    AuthInfo auth_info;
                    if (auto token_it = obj->find("token"); token_it != obj->end()) {
                        if (auto* s = token_it->get_if<std::string>()) {
                            auth_info.token = *s;
                            params.auth = auth_info;
                        }
                    }
                }
            }

            // Parse device
            if (auto it = json.find("device"); it != json.end()) {
                if (auto* obj = it->get_if<glz::json_t>()) {
                    DeviceInfo device_info;

                    if (auto id_it = obj->find("id"); id_it != obj->end()) {
                        if (auto* s = id_it->get_if<std::string>()) {
                            device_info.id = *s;
                        }
                    }

                    if (auto pk_it = obj->find("publicKey"); pk_it != obj->end()) {
                        if (auto* s = pk_it->get_if<std::string>()) {
                            device_info.public_key = *s;
                        }
                    }

                    if (auto sig_it = obj->find("signature"); sig_it != obj->end()) {
                        if (auto* s = sig_it->get_if<std::string>()) {
                            device_info.signature = *s;
                        }
                    }

                    if (auto sat_it = obj->find("signedAt"); sat_it != obj->end()) {
                        if (auto* n = sat_it->get_if<uint64_t>()) {
                            device_info.signed_at = *n;
                        }
                    }

                    if (auto nonce_it = obj->find("nonce"); nonce_it != obj->end()) {
                        if (auto* s = nonce_it->get_if<std::string>()) {
                            device_info.nonce = *s;
                        }
                    }

                    params.device = device_info;
                }
            }

            // Parse locale
            if (auto it = json.find("locale"); it != json.end()) {
                if (auto* s = it->get_if<std::string>()) {
                    params.locale = *s;
                }
            }

            // Parse user_agent
            if (auto it = json.find("userAgent"); it != json.end()) {
                if (auto* s = it->get_if<std::string>()) {
                    params.user_agent = *s;
                }
            }

            return params;
        } catch (...) {
            return std::nullopt;
        }
    }
};

/**
 * @brief connect.challenge event payload
 */
struct ConnectChallengePayload {
    std::string nonce;       // Random number (32 bytes)
    uint64_t timestamp;      // Timestamp (milliseconds)

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] auto to_json() const -> glz::json_t {
        glz::json_t json;
        json["nonce"] = nonce;
        json["ts"] = timestamp;
        return json;
    }

    /**
     * @brief Parse from JSON
     */
    static auto from_json(const glz::json_t& json) -> std::optional<ConnectChallengePayload> {
        try {
            ConnectChallengePayload payload;

            if (auto it = json.find("nonce"); it != json.end()) {
                if (auto* s = it->get_if<std::string>()) {
                    payload.nonce = *s;
                }
            }

            if (auto it = json.find("ts"); it != json.end()) {
                if (auto* n = it->get_if<uint64_t>()) {
                    payload.timestamp = *n;
                }
            }

            return payload;
        } catch (...) {
            return std::nullopt;
        }
    }
};

/**
 * @brief hello-ok response payload
 */
struct HelloOkPayload {
    std::string type = "hello-ok";
    uint32_t protocol = 1;

    struct Policy {
        uint64_t tick_interval_ms = 15000;
    };
    Policy policy;

    struct Auth {
        std::string device_token;
        std::string role;
        std::vector<std::string> scopes;
    };
    Auth auth;

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] auto to_json() const -> glz::json_t {
        glz::json_t json;
        json["type"] = type;
        json["protocol"] = protocol;

        glz::json_t policy_json;
        policy_json["tickIntervalMs"] = policy.tick_interval_ms;
        json["policy"] = policy_json;

        glz::json_t auth_json;
        auth_json["deviceToken"] = auth.device_token;
        auth_json["role"] = auth.role;
        auth_json["scopes"] = auth.scopes;
        json["auth"] = auth_json;

        return json;
    }
};

// ==================== Ping Method Schema ====================

/**
 * @brief ping request parameters
 */
struct PingParams {
    uint64_t timestamp;  // Client timestamp (milliseconds)

    /**
     * @brief Parse from JSON
     */
    static auto from_json(const glz::json_t& json) -> std::optional<PingParams> {
        try {
            PingParams params;

            if (auto it = json.find("timestamp"); it != json.end()) {
                if (auto* n = it->get_if<uint64_t>()) {
                    params.timestamp = *n;
                    return params;
                }
            }

            return std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }
};

/**
 * @brief ping response payload
 */
struct PingPayload {
    uint64_t timestamp;   // Server timestamp (milliseconds)
    uint64_t latency_ms;  // Round-trip latency (milliseconds)

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] auto to_json() const -> glz::json_t {
        glz::json_t json;
        json["timestamp"] = timestamp;
        json["latencyMs"] = latency_ms;
        return json;
    }
};

// ==================== Tick Event Schema ====================

/**
 * @brief tick event payload
 */
struct TickPayload {
    uint64_t timestamp;  // Timestamp (milliseconds)
    uint64_t seq;        // Sequence number

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] auto to_json() const -> glz::json_t {
        glz::json_t json;
        json["timestamp"] = timestamp;
        json["seq"] = seq;
        return json;
    }

    /**
     * @brief Parse from JSON
     */
    static auto from_json(const glz::json_t& json) -> std::optional<TickPayload> {
        try {
            TickPayload payload;

            if (auto it = json.find("timestamp"); it != json.end()) {
                if (auto* n = it->get_if<uint64_t>()) {
                    payload.timestamp = *n;
                }
            }

            if (auto it = json.find("seq"); it != json.end()) {
                if (auto* n = it->get_if<uint64_t>()) {
                    payload.seq = *n;
                }
            }

            return payload;
        } catch (...) {
            return std::nullopt;
        }
    }
};

// ==================== Validation Tools ====================

/**
 * @brief Schema validator
 */
class SchemaValidator {
public:
    /**
     * @brief Validate if role is valid
     */
    [[nodiscard]] static auto validate_role(std::string_view role) -> bool {
        return role == "operator" || role == "node";
    }

    /**
     * @brief Validate if scope is valid
     */
    [[nodiscard]] static auto validate_scope(std::string_view scope) -> bool {
        // operator.* scopes
        if (scope == "operator.read" ||
            scope == "operator.write" ||
            scope == "operator.admin" ||
            scope == "operator.approvals") {
            return true;
        }

        // Support wildcard
        if (scope == "operator.*") {
            return true;
        }

        return false;
    }

    /**
     * @brief Validate if cap is valid
     */
    [[nodiscard]] static auto validate_cap(std::string_view cap) -> bool {
        static const std::unordered_set<std::string> valid_caps = {
            "code-generation",
            "code-review",
            "debugging",
            "testing",
            "documentation",
        };

        return valid_caps.contains(std::string(cap));
    }

    /**
     * @brief Validate if command is valid
     */
    [[nodiscard]] static auto validate_command(std::string_view command) -> bool {
        static const std::unordered_set<std::string> valid_commands = {
            "task.execute",
            "task.cancel",
            "status.update",
        };

        return valid_commands.contains(std::string(command));
    }
};

} // namespace moltcat::protocol
