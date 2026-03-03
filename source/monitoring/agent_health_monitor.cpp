#include "agent/agent_health_monitor.hpp"
#include "messaging/message.hpp"
#include <algorithm>

namespace moltcat::agent {

// ================================
// Constructor and Destructor
// ================================

AgentHealthMonitor::AgentHealthMonitor(
    TeamRegistry* registry,
    messaging::MessageBusClient* message_bus_client,
    const MonitorConfig& config,
    std::shared_ptr<spdlog::logger> logger
)
    : registry_(registry)
    , message_bus_client_(message_bus_client)
    , config_(config)
    , logger_(logger ? logger : spdlog::default_logger())
{
    if (!registry_) {
        logger_->error("AgentHealthMonitor: TeamRegistry cannot be null");
        throw std::invalid_argument("registry cannot be null");
    }

    if (!message_bus_client_) {
        logger_->error("AgentHealthMonitor: MessageBusClient cannot be null");
        throw std::invalid_argument("message_bus_client cannot be null");
    }

    logger_->info("AgentHealthMonitor initialized: heartbeat_interval={}ms, timeout={}ms",
                  config_.heartbeat_interval_ms, config_.heartbeat_timeout_ms);
}

AgentHealthMonitor::~AgentHealthMonitor() {
    stop();
}

// ================================
// Lifecycle Management
// ================================

auto AgentHealthMonitor::start() -> void {
    if (running_.load()) {
        logger_->warn("AgentHealthMonitor already running");
        return;
    }

    running_.store(true);

    // Start monitoring thread
    monitor_thread_ = std::thread([this]() {
        this->monitor_loop();
    });

    logger_->info("AgentHealthMonitor started");
}

auto AgentHealthMonitor::stop() -> void {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    // Wait for monitoring thread to finish
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    logger_->info("AgentHealthMonitor stopped");
}

// ================================
// Health Status Query
// ================================

auto AgentHealthMonitor::get_health_status(const std::string& agent_id) const
    -> std::optional<AgentHealthStatus>
{
    std::lock_guard lock(health_mutex_);

    auto it = health_status_.find(agent_id);
    if (it != health_status_.end()) {
        return it->second;
    }

    return std::nullopt;
}

auto AgentHealthMonitor::get_suspected_agents() const -> std::vector<std::string> {
    std::lock_guard lock(health_mutex_);

    std::vector<std::string> suspected;
    for (const auto& [agent_id, status] : health_status_) {
        if (status == AgentHealthStatus::SUSPECTED) {
            suspected.push_back(agent_id);
        }
    }

    return suspected;
}

auto AgentHealthMonitor::get_dead_agents() const -> std::vector<std::string> {
    std::lock_guard lock(health_mutex_);

    std::vector<std::string> dead;
    for (const auto& [agent_id, status] : health_status_) {
        if (status == AgentHealthStatus::CONFIRMED_DEAD) {
            dead.push_back(agent_id);
        }
    }

    return dead;
}

// ================================
// Configuration
// ================================

auto AgentHealthMonitor::update_config(const MonitorConfig& config) -> void {
    config_ = config;
    logger_->info("AgentHealthMonitor configuration updated: heartbeat_interval={}ms, timeout={}ms",
                  config_.heartbeat_interval_ms, config_.heartbeat_timeout_ms);
}

// ================================
// Statistics
// ================================

auto AgentHealthMonitor::get_stats() const -> HealthStats {
    std::lock_guard lock(stats_mutex_);
    return stats_;
}

auto AgentHealthMonitor::reset_stats() -> void {
    std::lock_guard lock(stats_mutex_);
    stats_ = HealthStats{};
    logger_->info("AgentHealthMonitor statistics reset");
}

// ================================
// Internal Methods
// ================================

auto AgentHealthMonitor::monitor_loop() -> void {
    logger_->debug("AgentHealthMonitor monitoring thread started");

    while (running_.load()) {
        try {
            // 1. Send heartbeats
            send_heartbeats();

            // 2. Check timeouts
            check_timeouts();

            // 3. Clean up dead Agents
            if (config_.enable_auto_cleanup) {
                cleanup_dead_agents();
            }

        } catch (const std::exception& e) {
            logger_->error("AgentHealthMonitor monitoring loop exception: {}", e.what());
        }

        // Wait for next cycle
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeat_interval_ms));
    }

    logger_->debug("AgentHealthMonitor monitoring thread stopped");
}

auto AgentHealthMonitor::send_heartbeats() -> void {
    // TODO: Send heartbeats to all Agents via A2A
    //
    // Current implementation: simulated heartbeat sending
    //
    // Actual implementation should be:
    // for (const auto& [agent_id, info] : registry_->get_all_agents()) {
    //     a2a_client_->send_heartbeat(info.a2a_address);
    //     stats_.total_heartbeats_sent++;
    // }

    // Temporary implementation: do not send actual heartbeats, just log
    // logger_->debug("AgentHealthMonitor sending heartbeat (simulated)");
}

auto AgentHealthMonitor::check_timeouts() -> void {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard lock1(heartbeats_mutex_);
    std::lock_guard lock2(health_mutex_);

    for (auto& [agent_id, last_heartbeat] : heartbeats_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_heartbeat
        ).count();

        // Check if timeout
        if (elapsed > static_cast<int64_t>(config_.heartbeat_timeout_ms)) {
            auto old_status = health_status_[agent_id];

            if (old_status != AgentHealthStatus::CONFIRMED_DEAD) {
                // Mark as suspected
                health_status_[agent_id] = AgentHealthStatus::SUSPECTED;

                logger_->warn("Agent {} heartbeat timeout ({}ms), marked as suspected",
                              agent_id, elapsed);

                {
                    std::lock_guard lock(stats_mutex_);
                    stats_.suspected_count++;
                }

                // Broadcast suspected status
                broadcast_health_status(agent_id, AgentHealthStatus::SUSPECTED);

                // Invoke callback
                if (health_change_callback_) {
                    health_change_callback_(
                        agent_id,
                        old_status,
                        AgentHealthStatus::SUSPECTED
                    );
                }
            }
        }
    }
}

auto AgentHealthMonitor::cleanup_dead_agents() -> void {
    std::lock_guard lock(health_mutex_);

    auto it = health_status_.begin();
    while (it != health_status_.end()) {
        const auto& [agent_id, status] = *it;

        if (status == AgentHealthStatus::CONFIRMED_DEAD) {
            logger_->info("Agent {} confirmed dead, removing from Registry", agent_id);

            // Remove from TeamRegistry
            registry_->unregister_agent(agent_id);

            // Remove from heartbeat records
            {
                std::lock_guard lock2(heartbeats_mutex_);
                heartbeats_.erase(agent_id);
            }

            // Remove from health status
            it = health_status_.erase(it);

            {
                std::lock_guard lock2(stats_mutex_);
                stats_.auto_cleaned_count++;
            }

        } else {
            ++it;
        }
    }
}

auto AgentHealthMonitor::update_health_status(
    const std::string& agent_id,
    AgentHealthStatus new_status
) -> void {
    std::lock_guard lock(health_mutex_);

    auto old_status = health_status_[agent_id];

    if (old_status != new_status) {
        health_status_[agent_id] = new_status;

        logger_->info("Agent {} health status changed: {} -> {}",
                      agent_id,
                      health_status_to_string(old_status),
                      health_status_to_string(new_status));

        // Broadcast status change
        broadcast_health_status(agent_id, new_status);

        // Invoke callback
        if (health_change_callback_) {
            health_change_callback_(agent_id, old_status, new_status);
        }

        // Update statistics
        std::lock_guard lock2(stats_mutex_);
        if (new_status == AgentHealthStatus::CONFIRMED_DEAD) {
            stats_.confirmed_dead_count++;
        }
    }
}

auto AgentHealthMonitor::broadcast_health_status(
    const std::string& agent_id,
    AgentHealthStatus status
) -> void {
    // TODO: Broadcast health status via MessageBusClient
    //
    // Example code:
    // Message msg = Message::create(MessageType::AGENT_HEALTH_UPDATE, "health_monitor");
    // msg.header.destination = "agent_health";  // topic or service name
    // msg.body.content = glz::write_json({
    //     {"agent_id", agent_id},
    //     {"status", static_cast<uint32_t>(status)}
    // });
    // message_bus_client_->broadcast("agent_health", msg);

    logger_->debug("AgentHealthMonitor broadcasting health status: agent={}, status={}",
                   agent_id, static_cast<uint32_t>(status));
}

// ================================
// Helper Functions
// ================================

auto health_status_to_string(AgentHealthMonitor::AgentHealthStatus status)
    -> std::string
{
    switch (status) {
        case AgentHealthMonitor::AgentHealthStatus::HEALTHY:
            return "HEALTHY";
        case AgentHealthMonitor::AgentHealthStatus::SUSPECTED:
            return "SUSPECTED";
        case AgentHealthMonitor::AgentHealthStatus::CONFIRMED_DEAD:
            return "CONFIRMED_DEAD";
        default:
            return "UNKNOWN";
    }
}

} // namespace moltcat::agent
