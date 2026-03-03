#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <spdlog/spdlog.h>

namespace moltcat::messaging {

/**
 * @brief Service registry (in-memory storage)
 *
 * Responsibilities:
 * 1. Manage service name → queue ID list mapping
 * 2. Manage topic → subscriber queue ID list mapping
 * 3. Provide thread-safe query interface
 *
 * Design notes:
 * - In-memory storage, data lost after restart (simplified design)
 * - Use read-write lock to optimize read-many-write-few scenarios
 * - Support multiple instances of the same service (multiple queue IDs)
 */
class ServiceRegistry {
public:
    ServiceRegistry();
    ~ServiceRegistry() = default;

    // Disable copy and move
    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;
    ServiceRegistry(ServiceRegistry&&) = delete;
    ServiceRegistry& operator=(ServiceRegistry&&) = delete;

    /**
     * @brief Register service
     *
     * @param service_name Service name (e.g., "AgentTeam", "MemoryService")
     * @param queue_id Queue ID (ZeroMQ identity or UUID)
     * @return true indicates successful registration, false indicates queue already registered to other service
     */
    auto register_service(
        const std::string& service_name,
        const std::string& queue_id
    ) -> bool;

    /**
     * @brief Unregister service
     *
     * @param service_name Service name
     * @param queue_id Queue ID
     * @return true indicates successful unregistration, false indicates service does not exist
     */
    auto unregister_service(
        const std::string& service_name,
        const std::string& queue_id
    ) -> bool;

    /**
     * @brief Get all queues for a service
     *
     * @param service_name Service name
     * @return Queue ID list (returns empty list if service does not exist)
     */
    auto get_service_queues(
        const std::string& service_name
    ) const -> std::vector<std::string>;

    /**
     * @brief Check if service exists
     *
     * @param service_name Service name
     * @return true indicates service exists and has at least one instance
     */
    auto has_service(const std::string& service_name) const -> bool;

    /**
     * @brief Get all registered service names
     *
     * @return Service name list
     */
    auto get_all_services() const -> std::vector<std::string>;

    /**
     * @brief Subscribe to topic
     *
     * @param queue_id Subscriber queue ID
     * @param topic Topic name
     */
    auto subscribe(
        const std::string& queue_id,
        const std::string& topic
    ) -> void;

    /**
     * @brief Unsubscribe
     *
     * @param queue_id Subscriber queue ID
     * @param topic Topic name
     */
    auto unsubscribe(
        const std::string& queue_id,
        const std::string& topic
    ) -> void;

    /**
     * @brief Get all subscribers for a topic
     *
     * @param topic Topic name
     * @return Subscriber queue ID list
     */
    auto get_subscribers(const std::string& topic) const
        -> std::vector<std::string>;

    /**
     * @brief Check if queue is subscribed to topic
     *
     * @param queue_id Queue ID
     * @param topic Topic name
     * @return true indicates subscribed
     */
    auto is_subscribed(
        const std::string& queue_id,
        const std::string& topic
    ) const -> bool;

    /**
     * @brief Get service that queue belongs to
     *
     * @param queue_id Queue ID
     * @return Service name (returns empty string if queue does not exist)
     */
    auto get_queue_service(const std::string& queue_id) const
        -> std::string;

    /**
     * @brief Get statistics
     */
    struct RegistryStats {
        size_t total_services = 0;
        size_t total_queues = 0;
        size_t total_subscriptions = 0;
    };

    auto get_stats() const -> RegistryStats;

    /**
     * @brief Clear all registration information (mainly for testing)
     */
    auto clear() -> void;

private:
    // Service name → queue ID list
    std::unordered_map<std::string, std::vector<std::string>> services_;

    // Topic → subscriber queue ID list
    std::unordered_map<std::string, std::vector<std::string>> subscriptions_;

    // Queue ID → service name (reverse mapping)
    std::unordered_map<std::string, std::string> queue_to_service_;

    // Read-write lock (for thread safety)
    mutable std::shared_mutex mutex_;

    // Logger
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace moltcat::messaging
