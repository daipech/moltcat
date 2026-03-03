#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>

namespace moltcat::protocol {

/**
 * @brief WebSocket method list
 *
 * Defines all supported methods and their permission requirements
 */
namespace ws_methods {

// ==================== Common Methods ====================

/// Heartbeat detection
inline constexpr std::string_view PING = "ping";

/// Get online device list
inline constexpr std::string_view SYSTEM_PRESENCE = "system.presence";

/// Get system status
inline constexpr std::string_view SYSTEM_STATUS = "system.status";

// ==================== Connection Methods ====================

/// Connection authentication
inline constexpr std::string_view CONNECT = "connect";

// ==================== Task Management ====================

/// Create task
inline constexpr std::string_view TASK_CREATE = "task.create";

/// Get task details
inline constexpr std::string_view TASK_GET = "task.get";

/// List tasks
inline constexpr std::string_view TASK_LIST = "task.list";

/// Cancel task
inline constexpr std::string_view TASK_CANCEL = "task.cancel";

/// Agent executes task (node only)
inline constexpr std::string_view TASK_EXECUTE = "task.execute";

/// Update task status (node only)
inline constexpr std::string_view TASK_UPDATE = "task.update";

// ==================== Agent Management ====================

/// List agents
inline constexpr std::string_view AGENT_LIST = "agent.list";

/// Get agent details
inline constexpr std::string_view AGENT_GET = "agent.get";

/// Register agent (node only)
inline constexpr std::string_view AGENT_REGISTER = "agent.register";

/// Unregister agent
inline constexpr std::string_view AGENT_UNREGISTER = "agent.unregister";

// ==================== Device Management ====================

/// List devices
inline constexpr std::string_view DEVICE_LIST = "device.list";

/// Rotate device token
inline constexpr std::string_view DEVICE_TOKEN_ROTATE = "device.token.rotate";

/// Revoke device token
inline constexpr std::string_view DEVICE_TOKEN_REVOKE = "device.token.revoke";

} // namespace ws_methods

// ==================== Event List ====================

namespace ws_events {

/// Connection challenge
inline constexpr std::string_view CONNECT_CHALLENGE = "connect.challenge";

/// Server heartbeat
inline constexpr std::string_view TICK = "tick";

/// Task completed event
inline constexpr std::string_view TASK_COMPLETED = "task.completed";

/// Task failed event
inline constexpr std::string_view TASK_FAILED = "task.failed";

/// Agent online event
inline constexpr std::string_view AGENT_ONLINE = "agent.online";

/// Agent offline event
inline constexpr std::string_view AGENT_OFFLINE = "agent.offline";

} // namespace ws_events

/**
 * @brief Method permission checker
 */
class MethodRegistry {
public:
    /**
     * @brief Check if method exists
     */
    [[nodiscard]] static auto has_method(std::string_view method) -> bool {
        static const std::unordered_set<std::string> methods = {
            // Common
            std::string(ws_methods::PING),
            std::string(ws_methods::SYSTEM_PRESENCE),
            std::string(ws_methods::SYSTEM_STATUS),

            // Connection
            std::string(ws_methods::CONNECT),

            // Task
            std::string(ws_methods::TASK_CREATE),
            std::string(ws_methods::TASK_GET),
            std::string(ws_methods::TASK_LIST),
            std::string(ws_methods::TASK_CANCEL),
            std::string(ws_methods::TASK_EXECUTE),
            std::string(ws_methods::TASK_UPDATE),

            // Agent
            std::string(ws_methods::AGENT_LIST),
            std::string(ws_methods::AGENT_GET),
            std::string(ws_methods::AGENT_REGISTER),
            std::string(ws_methods::AGENT_UNREGISTER),

            // Device
            std::string(ws_methods::DEVICE_LIST),
            std::string(ws_methods::DEVICE_TOKEN_ROTATE),
            std::string(ws_methods::DEVICE_TOKEN_REVOKE),
        };

        return methods.contains(std::string(method));
    }

    /**
     * @brief Check if event exists
     */
    [[nodiscard]] static auto has_event(std::string_view event) -> bool {
        static const std::unordered_set<std::string> events = {
            std::string(ws_events::CONNECT_CHALLENGE),
            std::string(ws_events::TICK),
            std::string(ws_events::TASK_COMPLETED),
            std::string(ws_events::TASK_FAILED),
            std::string(ws_events::AGENT_ONLINE),
            std::string(ws_events::AGENT_OFFLINE),
        };

        return events.contains(std::string(event));
    }

    /**
     * @brief Get all method list
     */
    [[nodiscard]] static auto get_all_methods() -> std::vector<std::string> {
        return {
            // Common
            std::string(ws_methods::PING),
            std::string(ws_methods::SYSTEM_PRESENCE),
            std::string(ws_methods::SYSTEM_STATUS),

            // Connection
            std::string(ws_methods::CONNECT),

            // Task
            std::string(ws_methods::TASK_CREATE),
            std::string(ws_methods::TASK_GET),
            std::string(ws_methods::TASK_LIST),
            std::string(ws_methods::TASK_CANCEL),
            std::string(ws_methods::TASK_EXECUTE),
            std::string(ws_methods::TASK_UPDATE),

            // Agent
            std::string(ws_methods::AGENT_LIST),
            std::string(ws_methods::AGENT_GET),
            std::string(ws_methods::AGENT_REGISTER),
            std::string(ws_methods::AGENT_UNREGISTER),

            // Device
            std::string(ws_methods::DEVICE_LIST),
            std::string(ws_methods::DEVICE_TOKEN_ROTATE),
            std::string(ws_methods::DEVICE_TOKEN_REVOKE),
        };
    }

    /**
     * @brief Get all event list
     */
    [[nodiscard]] static auto get_all_events() -> std::vector<std::string> {
        return {
            std::string(ws_events::CONNECT_CHALLENGE),
            std::string(ws_events::TICK),
            std::string(ws_events::TASK_COMPLETED),
            std::string(ws_events::TASK_FAILED),
            std::string(ws_events::AGENT_ONLINE),
            std::string(ws_events::AGENT_OFFLINE),
        };
    }
};

} // namespace moltcat::protocol
