#pragma once

#include "agent_health_monitor.hpp"
#include "smart_agent_router.hpp"
#include <memory>
#include <string>
#include <glaze/json.hpp>

namespace moltcat::monitoring {

/**
 * UnifiedMonitor - Unified monitoring interface
 *
 * Responsibilities:
 * 1. Collect A2A communication metrics
 * 2. Collect MessageBus communication metrics
 * 3. Correlation analysis (correlation_id)
 * 4. Generate unified monitoring reports
 *
 * Monitoring metrics:
 * - A2A: Latency, throughput, error rate
 * - MessageBus: Message backlog, processing time
 * - Agent: Health status, resource usage
 */
class UnifiedMonitor {
public:
    // ================================
    // Monitoring metric types
    // ================================

    /**
     * A2A communication metrics
     */
    struct A2AMetrics {
        uint64_t total_messages = 0;           // Total messages
        uint64_t successful_messages = 0;      // Successful messages
        uint64_t failed_messages = 0;          // Failed messages
        double avg_latency_ms = 0.0;           // Average latency (milliseconds)
        double min_latency_ms = 0.0;           // Minimum latency
        double max_latency_ms = 0.0;           // Maximum latency
        uint64_t throughput_msg_per_sec = 0;   // Throughput (messages/second)
        double error_rate = 0.0;               // Error rate (0-1)
    };

    /**
     * MessageBus communication metrics
     */
    struct MessageBusMetrics {
        uint64_t total_messages = 0;           // Total messages
        uint64_t queued_messages = 0;          // Queued messages
        uint64_t processed_messages = 0;       // Processed messages
        double avg_processing_time_ms = 0.0;   // Average processing time
        uint64_t handler_count = 0;            // Registered handler count
        uint64_t queue_count = 0;              // Queue count
    };

    /**
     * Agent health metrics
     */
    struct AgentHealthMetrics {
        uint64_t total_agents = 0;             // Total Agent count
        uint64_t healthy_agents = 0;           // Healthy Agent count
        uint64_t suspected_agents = 0;         // Suspected Agent count
        uint64_t dead_agents = 0;              // Dead Agent count
    };

    /**
     * Unified monitoring report
     */
    struct MonitoringReport {
        std::string report_id;                 // Report ID
        uint64_t timestamp_ms;                 // Timestamp
        std::string correlation_id;            // Correlation ID

        A2AMetrics a2a_metrics;
        MessageBusMetrics messagebus_metrics;
        AgentHealthMetrics agent_health_metrics;

        // Serialize to JSON
        auto serialize() const -> glz::json_t;
    };

    // ================================
    // Constructors
    // ================================

    /**
     * Constructor
     */
    UnifiedMonitor() = default;

    ~UnifiedMonitor() = default;

    // ================================
    // Metric collection
    // ================================

    /**
     * Collect all monitoring metrics
     *
     * @return Monitoring report
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto collect_metrics() const -> MonitoringReport;

    /**
     * Collect A2A metrics
     *
     * @param router SmartAgentRouter pointer (optional)
     * @return A2A metrics
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto collect_a2a_metrics(
        const agent::SmartAgentRouter* router = nullptr
    ) const -> A2AMetrics;

    /**
     * Collect MessageBus metrics
     *
     * @param message_bus MessageBus pointer (optional)
     * @return MessageBus metrics
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto collect_messagebus_metrics(
        const messaging::MessageBus* message_bus = nullptr
    ) const -> MessageBusMetrics;

    /**
     * Collect Agent health metrics
     *
     * @param health_monitor AgentHealthMonitor pointer (optional)
     * @return Agent health metrics
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto collect_agent_health_metrics(
        const agent::AgentHealthMonitor* health_monitor = nullptr
    ) const -> AgentHealthMetrics;

    // ================================
    // Monitoring component registration
    // ================================

    /**
     * Set SmartAgentRouter (for collecting A2A metrics)
     */
    auto set_router(const agent::SmartAgentRouter* router) -> void {
        router_ = router;
    }

    /**
     * Set MessageBus (for collecting MessageBus metrics)
     */
    auto set_message_bus(const messaging::MessageBus* message_bus) -> void {
        message_bus_ = message_bus;
    }

    /**
     * Set AgentHealthMonitor (for collecting health metrics)
     */
    auto set_health_monitor(const agent::AgentHealthMonitor* monitor) -> void {
        health_monitor_ = monitor;
    }

    // ================================
    // Report generation
    // ================================

    /**
     * Generate JSON report
     *
     * @return JSON string
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto generate_json_report() const -> std::string;

    /**
     * Generate human-readable report
     *
     * @return Formatted string
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto generate_readable_report() const -> std::string;

private:
    // ================================
    // Fields
    // ================================

    const agent::SmartAgentRouter* router_ = nullptr;
    const messaging::MessageBus* message_bus_ = nullptr;
    const agent::AgentHealthMonitor* health_monitor_ = nullptr;
};

} // namespace moltcat::monitoring
