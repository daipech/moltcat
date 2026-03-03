#pragma once

#include "team_registry.hpp"
#include "messaging/message_bus.hpp"
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <spdlog/spdlog.h>

namespace moltcat::agent {

/**
 * AgentHealthMonitor - Agent health monitor
 *
 * Responsibilities:
 * 1. A2A heartbeat monitoring (high-frequency detection, fast failure discovery)
 * 2. MessageBus status broadcast (authoritative confirmation)
 * 3. Network partition handling
 * 4. Automatic failover
 *
 * Dual-layer detection mechanism:
 * - Fast detection (A2A heartbeat): suspect Agent failure (suspected status)
 * - Authoritative confirmation (MessageBus): confirm Agent failure (offline status)
 *
 * Workflow:
 * 1. A2A heartbeat timeout → mark as suspected → broadcast AGENT_SUSPECTED
 * 2. MessageBus confirmation → mark as offline → broadcast AGENT_CONFIRMED_DEAD
 * 3. Automatic cleanup → remove from TeamRegistry
 *
 * Configuration:
 * - heartbeat_interval_ms: heartbeat interval (default 1000ms)
 * - heartbeat_timeout_ms: heartbeat timeout (default 3000ms)
 * - cleanup_timeout_ms: cleanup timeout (default 10000ms)
 */
class AgentHealthMonitor {
public:
    // ================================
    // Type Definitions
    // ================================

    /**
     * Agent health status
     */
    enum class AgentHealthStatus : uint32_t {
        HEALTHY = 0,         // Healthy (normal)
        SUSPECTED = 1,       // Suspected (heartbeat timeout, not confirmed)
        CONFIRMED_DEAD = 2   // Confirmed dead (MessageBus confirmed)
    };

    /**
     * Health status change callback
     */
    using HealthChangeCallback = std::function<void(
        const std::string& agent_id,
        AgentHealthStatus old_status,
        AgentHealthStatus new_status
    )>;

    /**
     * Monitor configuration
     */
    struct MonitorConfig {
        uint64_t heartbeat_interval_ms = 1000;      // Heartbeat interval (milliseconds)
        uint64_t heartbeat_timeout_ms = 3000;      // Heartbeat timeout (milliseconds)
        uint64_t cleanup_timeout_ms = 10000;       // Cleanup timeout (milliseconds)
        bool enable_auto_cleanup = true;           // Enable automatic cleanup
    };

    // ================================
    // Constructors
    // ================================

    /**
     * Constructor
     *
     * @param registry TeamRegistry reference
     * @param message_bus_client MessageBusClient reference
     * @param config Monitor configuration
     * @param logger Logger
     */
    AgentHealthMonitor(
        TeamRegistry* registry,
        messaging::MessageBusClient* message_bus_client,
        const MonitorConfig& config = {},
        std::shared_ptr<spdlog::logger> logger = nullptr
    );

    ~AgentHealthMonitor();

    // Disable copy and move
    AgentHealthMonitor(const AgentHealthMonitor&) = delete;
    AgentHealthMonitor& operator=(const AgentHealthMonitor&) = delete;
    AgentHealthMonitor(AgentHealthMonitor&&) = delete;
    AgentHealthMonitor& operator=(AgentHealthMonitor&&) = delete;

    // ================================
    // Lifecycle Management
    // ================================

    /**
     * Start monitoring
     *
     * @note Start background monitoring thread
     */
    auto start() -> void;

    /**
     * Stop monitoring
     *
     * @note Stop background monitoring thread
     */
    auto stop() -> void;

    /**
     * Get running status
     */
    [[nodiscard]] auto is_running() const noexcept -> bool {
        return running_.load();
    }

    // ================================
    // Health Status Query
    // ================================

    /**
     * Get Agent health status
     *
     * @param agent_id Agent ID
     * @return Health status (returns nullopt if not found)
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto get_health_status(const std::string& agent_id) const
        -> std::optional<AgentHealthStatus>;

    /**
     * Get all suspected Agents
     *
     * @return Agent ID list
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto get_suspected_agents() const -> std::vector<std::string>;

    /**
     * Get all confirmed dead Agents
     *
     * @return Agent ID list
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto get_dead_agents() const -> std::vector<std::string>;

    // ================================
    // Configuration
    // ================================

    /**
     * Set health status change callback
     *
     * @param callback Callback function
     *
     * @note Called when Agent health status changes
     */
    auto set_health_change_callback(HealthChangeCallback callback) -> void {
        health_change_callback_ = std::move(callback);
    }

    /**
     * Get configuration
     */
    [[nodiscard]] auto get_config() const noexcept -> const MonitorConfig& {
        return config_;
    }

    /**
     * Update configuration
     *
     * @param config New configuration
     *
     * @note Dynamically update monitoring parameters
     */
    auto update_config(const MonitorConfig& config) -> void;

    // ================================
    // Statistics
    // ================================

    /**
     * Statistics
     */
    struct HealthStats {
        uint64_t total_heartbeats_sent = 0;      // Total heartbeats sent
        uint64_t total_heartbeats_received = 0;  // Total heartbeats received
        uint64_t suspected_count = 0;            // Suspected Agent count
        uint64_t confirmed_dead_count = 0;       // Confirmed dead count
        uint64_t auto_cleaned_count = 0;         // Auto cleaned count
    };

    /**
     * Get statistics
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto get_stats() const -> HealthStats;

    /**
     * Reset statistics
     *
     * @note Thread-safe
     */
    auto reset_stats() -> void;

private:
    /**
     * Monitor thread main loop
     */
    auto monitor_loop() -> void;

    /**
     * Send heartbeats to all Agents
     */
    auto send_heartbeats() -> void;

    /**
     * Check heartbeat timeouts
     */
    auto check_timeouts() -> void;

    /**
     * Clean up dead Agents
     */
    auto cleanup_dead_agents() -> void;

    /**
     * Update Agent health status
     *
     * @param agent_id Agent ID
     * @param new_status New status
     */
    auto update_health_status(
        const std::string& agent_id,
        AgentHealthStatus new_status
    ) -> void;

    /**
     * Broadcast Agent health status change
     */
    auto broadcast_health_status(
        const std::string& agent_id,
        AgentHealthStatus status
    ) -> void;

    // ================================
    // Fields
    // ================================

    TeamRegistry* registry_;                                   // TeamRegistry reference
    messaging::MessageBusClient* message_bus_client_;       // MessageBusClient reference
    MonitorConfig config_;                                    // Monitor configuration

    std::shared_ptr<spdlog::logger> logger_;             // Logger

    // Running status
    std::atomic<bool> running_{false};
    std::thread monitor_thread_;

    // Agent health status (agent_id -> status)
    std::unordered_map<std::string, AgentHealthStatus> health_status_;
    mutable std::mutex health_mutex_;

    // Heartbeat timestamps (agent_id -> last_heartbeat_time)
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> heartbeats_;
    std::mutex heartbeats_mutex_;

    // Callbacks
    HealthChangeCallback health_change_callback_;

    // Statistics
    mutable std::mutex stats_mutex_;
    HealthStats stats_;
};

/**
 * Helper function: convert health status to string
 */
auto health_status_to_string(AgentHealthMonitor::AgentHealthStatus status)
    -> std::string;

} // namespace moltcat::agent
