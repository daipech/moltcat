#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <optional>
#include "messaging/message.hpp"
#include <spdlog/spdlog.h>

namespace moltcat::agent {

/**
 * TeamRegistry - Agent team registry
 *
 * Responsibilities:
 * 1. Manage Agent team membership
 * 2. Maintain Team member lists
 * 3. Provide thread-safe query interfaces
 * 4. Support Agent status updates
 *
 * Design principles:
 * - Single Team membership: each Agent can only belong to one Team
 * - Thread-safe: internal state protected by shared_mutex
 * - Efficient query: dual indexing by team_id and agent_id
 */
class TeamRegistry {
public:
    // ================================
    // Constructors
    // ================================
    explicit TeamRegistry(std::shared_ptr<spdlog::logger> logger = nullptr);

    // ================================
    // Agent Registration Management
    // ================================

    /**
     * Register Agent to Team
     *
     * @param info Agent information (including agent_id, team_id, a2a_address, etc.)
     *
     * @note If Agent already exists, updates its information
     * @note Thread-safe
     */
    auto register_agent(const messaging::AgentInfo& info) -> void;

    /**
     * Remove Agent from Team
     *
     * @param agent_id Agent ID
     *
     * @note Searches and removes from all Teams
     * @note Thread-safe
     */
    auto unregister_agent(const std::string& agent_id) -> void;

    /**
     * Update Agent heartbeat time
     *
     * @param agent_id Agent ID
     *
     * @note Updates last heartbeat timestamp to current time
     * @note Thread-safe
     */
    auto update_heartbeat(const std::string& agent_id) -> void;

    /**
     * Update Agent status
     *
     * @param agent_id Agent ID
     * @param status New status
     *
     * @note Thread-safe
     */
    auto update_status(const std::string& agent_id, messaging::AgentStatus status) -> void;

    // ================================
    // Query Interfaces
    // ================================

    /**
     * Get all Team members
     *
     * @param team_id Team ID
     * @return Agent information list (empty array means Team does not exist or has no members)
     *
     * @note Thread-safe (shared lock)
     */
    auto get_team_members(const std::string& team_id) const
        -> std::vector<messaging::AgentInfo>;

    /**
     * Get single Agent information
     *
     * @param agent_id Agent ID
     * @return Agent information (returns nullopt if not found)
     *
     * @note Thread-safe (shared lock)
     */
    auto get_agent_info(const std::string& agent_id) const
        -> std::optional<messaging::AgentInfo>;

    /**
     * Get Team ID of Agent
     *
     * @param agent_id Agent ID
     * @return Team ID (returns nullopt if not found)
     *
     * @note Thread-safe (shared lock)
     */
    auto get_agent_team(const std::string& agent_id) const
        -> std::optional<std::string>;

    /**
     * Check if Team exists
     *
     * @param team_id Team ID
     * @return true indicates Team exists and has members
     *
     * @note Thread-safe (shared lock)
     */
    auto team_exists(const std::string& team_id) const -> bool;

    /**
     * Get all Teams list
     *
     * @return Team ID list
     *
     * @note Thread-safe (shared lock)
     */
    auto get_all_teams() const -> std::vector<std::string>;

    /**
     * Get Team member count
     *
     * @param team_id Team ID
     * @return Member count (returns 0 if Team does not exist)
     *
     * @note Thread-safe (shared lock)
     */
    auto get_team_size(const std::string& team_id) const -> size_t;

    // ================================
    // Cleanup Interfaces
    // ================================

    /**
     * Clean up offline Agents (based on heartbeat time)
     *
     * @param timeout_ms Heartbeat timeout (milliseconds)
     * @return Number of Agents cleaned up
     *
     * @note Removes Agents that have not been updated beyond timeout
     * @note Thread-safe
     */
    auto cleanup_stale_agents(uint64_t timeout_ms) -> size_t;

    /**
     * Clear all registration information
     *
     * @note Mainly for testing
     * @note Thread-safe
     */
    auto clear() -> void;

private:
    // ================================
    // Internal Data Structures
    // ================================

    /**
     * Team information
     */
    struct TeamInfo {
        std::unordered_map<std::string, messaging::AgentInfo> members;  // agent_id -> AgentInfo
        std::string a2a_topic;  // A2A PUB-SUB topic (e.g., "team_a.updates")
    };

    // ================================
    // Fields
    // ================================

    std::unordered_map<std::string, TeamInfo> teams_;  // team_id -> TeamInfo
    mutable std::shared_mutex mutex_;                  // Read-write lock
    std::shared_ptr<spdlog::logger> logger_;           // Logger
};

} // namespace moltcat::agent
