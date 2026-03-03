#include "agent/team_registry.hpp"
#include "messaging/message.hpp"
#include <algorithm>

namespace moltcat::agent {

// ================================
// Constructor
// ================================

TeamRegistry::TeamRegistry(std::shared_ptr<spdlog::logger> logger)
    : logger_(logger ? logger : spdlog::default_logger())
{
    logger_->info("TeamRegistry initialized");
}

// ================================
// Agent Registration Management
// ================================

auto TeamRegistry::register_agent(const messaging::AgentInfo& info) -> void {
    std::unique_lock lock(mutex_);

    // Check if Agent already registered (if registered, remove from old Team)
    for (auto& [team_id, team] : teams_) {
        if (team.members.erase(info.agent_id) > 0) {
            logger_->info("Agent {} moved from team {} to team {}",
                info.agent_id, team_id, info.team_id);
            break;
        }
    }

    // Register to new Team
    auto& team = teams_[info.team_id];
    team.members[info.agent_id] = info;

    // If this is Team's first member, set A2A topic
    if (team.members.size() == 1) {
        team.a2a_topic = info.team_id + ".updates";
        logger_->info("Team {} created with A2A topic: {}",
            info.team_id, team.a2a_topic);
    }

    logger_->info("Agent {} registered to team {} at {}",
        info.agent_id, info.team_id, info.a2a_address);
}

auto TeamRegistry::unregister_agent(const std::string& agent_id) -> void {
    std::unique_lock lock(mutex_);

    // Find and remove from all Teams
    for (auto& [team_id, team] : teams_) {
        if (team.members.erase(agent_id) > 0) {
            logger_->info("Agent {} unregistered from team {}",
                agent_id, team_id);

            // If Team is empty, can choose to delete Team
            if (team.members.empty()) {
                teams_.erase(team_id);
                logger_->info("Team {} removed (no members)", team_id);
            }
            break;
        }
    }
}

auto TeamRegistry::update_heartbeat(const std::string& agent_id) -> void {
    std::unique_lock lock(mutex_);

    // Find Agent and update heartbeat time
    for (auto& [_, team] : teams_) {
        auto it = team.members.find(agent_id);
        if (it != team.members.end()) {
            it->second.last_heartbeat = messaging::Message::current_timestamp();
            return;
        }
    }

    // Agent does not exist, log warning
    logger_->warn("Attempted to update heartbeat for unknown agent: {}", agent_id);
}

auto TeamRegistry::update_status(const std::string& agent_id,
                                 messaging::AgentStatus status) -> void {
    std::unique_lock lock(mutex_);

    // Find Agent and update status
    for (auto& [_, team] : teams_) {
        auto it = team.members.find(agent_id);
        if (it != team.members.end()) {
            it->second.status = status;
            logger_->debug("Agent {} status updated to {:d}",
                agent_id, static_cast<uint32_t>(status));
            return;
        }
    }

    // Agent does not exist, log warning
    logger_->warn("Attempted to update status for unknown agent: {}", agent_id);
}

// ================================
// Query Interfaces
// ================================

auto TeamRegistry::get_team_members(const std::string& team_id) const
    -> std::vector<messaging::AgentInfo>
{
    std::shared_lock lock(mutex_);

    auto it = teams_.find(team_id);
    if (it == teams_.end()) {
        return {};
    }

    std::vector<messaging::AgentInfo> members;
    members.reserve(it->second.members.size());

    for (const auto& [_, info] : it->second.members) {
        members.push_back(info);
    }

    return members;
}

auto TeamRegistry::get_agent_info(const std::string& agent_id) const
    -> std::optional<messaging::AgentInfo>
{
    std::shared_lock lock(mutex_);

    for (const auto& [_, team] : teams_) {
        auto it = team.members.find(agent_id);
        if (it != team.members.end()) {
            return it->second;
        }
    }

    return std::nullopt;
}

auto TeamRegistry::get_agent_team(const std::string& agent_id) const
    -> std::optional<std::string>
{
    std::shared_lock lock(mutex_);

    for (const auto& [team_id, team] : teams_) {
        if (team.members.contains(agent_id)) {
            return team_id;
        }
    }

    return std::nullopt;
}

auto TeamRegistry::team_exists(const std::string& team_id) const -> bool {
    std::shared_lock lock(mutex_);
    return teams_.contains(team_id) && !teams_.at(team_id).members.empty();
}

auto TeamRegistry::get_all_teams() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);

    std::vector<std::string> team_ids;
    team_ids.reserve(teams_.size());

    for (const auto& [team_id, _] : teams_) {
        team_ids.push_back(team_id);
    }

    return team_ids;
}

auto TeamRegistry::get_team_size(const std::string& team_id) const -> size_t {
    std::shared_lock lock(mutex_);

    auto it = teams_.find(team_id);
    if (it == teams_.end()) {
        return 0;
    }

    return it->second.members.size();
}

// ================================
// Cleanup Interfaces
// ================================

auto TeamRegistry::cleanup_stale_agents(uint64_t timeout_ms) -> size_t {
    std::unique_lock lock(mutex_);

    uint64_t current_time = messaging::Message::current_timestamp();
    size_t cleaned_count = 0;

    // Iterate through all Teams
    for (auto it = teams_.begin(); it != teams_.end(); ) {
        auto& [team_id, team] = *it;

        // Iterate through all Agents in Team
        for (auto agent_it = team.members.begin(); agent_it != team.members.end(); ) {
            const auto& [agent_id, info] = *agent_it;

            // Check if heartbeat timed out
            if (current_time - info.last_heartbeat > timeout_ms) {
                logger_->info("Cleaning up stale agent {} (last heartbeat {} ms ago)",
                    agent_id, current_time - info.last_heartbeat);
                agent_it = team.members.erase(agent_it);
                cleaned_count++;
            } else {
                ++agent_it;
            }
        }

        // If Team is empty, delete Team
        if (team.members.empty()) {
            logger_->info("Team {} removed (no members after cleanup)", team_id);
            it = teams_.erase(it);
        } else {
            ++it;
        }
    }

    if (cleaned_count > 0) {
        logger_->info("Cleaned up {} stale agents", cleaned_count);
    }

    return cleaned_count;
}

auto TeamRegistry::clear() -> void {
    std::unique_lock lock(mutex_);
    teams_.clear();
    logger_->info("TeamRegistry cleared");
}

} // namespace moltcat::agent
