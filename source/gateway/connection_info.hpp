#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <thread>

namespace moltcat::gateway {

/**
 * @brief Connection information
 *
 * Stores connection information for authenticated clients
 */
struct ConnectionInfo {
    std::string connection_id;      // Connection ID
    std::string device_id;          // Device ID
    std::string role;               // operator | node
    std::vector<std::string> scopes;       // Permission scopes
    std::vector<std::string> caps;         // Capability categories (node-specific)
    std::vector<std::string> commands;     // Command whitelist (node-specific)
    std::map<std::string, bool> permissions;  // Fine-grained permissions

    std::string client_id;          // Client ID (e.g., moltcat-cli)
    std::string client_version;     // Client version
    std::string platform;           // Platform (linux, windows, darwin)

    std::chrono::system_clock::time_point connected_at;
    std::chrono::system_clock::time_point last_activity;

    // Protocol information
    uint32_t protocol_version = 1;
    std::string device_token;
};

/**
 * @brief Gateway configuration base class
 */
struct GatewayConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;

    // Authentication configuration
    bool require_auth = true;
    bool enable_device_auth = true;  // Device signature verification

    // Heartbeat configuration
    size_t heartbeat_interval_ms = 30000;    // Client heartbeat interval
    size_t heartbeat_timeout_ms = 90000;     // Heartbeat timeout (3x heartbeat interval)
};

} // namespace moltcat::gateway
