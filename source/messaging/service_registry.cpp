#include "service_registry.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace moltcat::messaging {

// ================================
// Constructor
// ================================

ServiceRegistry::ServiceRegistry()
    : logger_(spdlog::get("moltcat"))
{
    if (!logger_) {
        logger_ = spdlog::default_logger();
    }
    logger_->info("Service registry initialized");
}

// ================================
// Register service
// ================================

auto ServiceRegistry::register_service(
    const std::string& service_name,
    const std::string& queue_id
) -> bool {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Check if queue is already registered to other service
    auto it = queue_to_service_.find(queue_id);
    if (it != queue_to_service_.end()) {
        if (it->second != service_name) {
            logger_->error("Queue {} already registered to service {}, cannot register to {}",
                queue_id, it->second, service_name);
            return false;
        }
        // Already registered to same service, idempotent operation
        logger_->warn("Queue {} already registered to service {} (idempotent operation)",
            queue_id, service_name);
        return true;
    }

    // Add to service mapping
    services_[service_name].push_back(queue_id);

    // Add reverse mapping
    queue_to_service_[queue_id] = service_name;

    logger_->info("Service registered successfully: {}, queue: {} (current instance count: {})",
        service_name, queue_id, services_[service_name].size());

    return true;
}

// ================================
// Unregister service
// ================================

auto ServiceRegistry::unregister_service(
    const std::string& service_name,
    const std::string& queue_id
) -> bool {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Find service
    auto service_it = services_.find(service_name);
    if (service_it == services_.end()) {
        logger_->warn("Service does not exist: {}", service_name);
        return false;
    }

    // Find and remove queue ID
    auto& queues = service_it->second;
    auto queue_it = std::find(queues.begin(), queues.end(), queue_id);
    if (queue_it == queues.end()) {
        logger_->warn("Queue {} does not belong to service {}", queue_id, service_name);
        return false;
    }

    queues.erase(queue_it);

    // Remove reverse mapping
    queue_to_service_.erase(queue_id);

    // If all queues of service are unregistered, remove service
    if (queues.empty()) {
        services_.erase(service_it);
        logger_->info("All instances of service {} unregistered, removing service", service_name);
    } else {
        logger_->info("Queue {} of service {} unregistered (remaining instances: {})",
            service_name, queue_id, queues.size());
    }

    // Clean up all subscriptions for this queue
    for (auto& [topic, subscribers] : subscriptions_) {
        auto sub_it = std::find(subscribers.begin(), subscribers.end(), queue_id);
        if (sub_it != subscribers.end()) {
            subscribers.erase(sub_it);
            logger_->debug("Cleaned up topic {} subscription for queue {}", queue_id, topic);
        }
    }

    return true;
}

// ================================
// Get all queues for a service
// ================================

auto ServiceRegistry::get_service_queues(
    const std::string& service_name
) const -> std::vector<std::string> {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = services_.find(service_name);
    if (it == services_.end()) {
        return {};
    }

    return it->second;
}

// ================================
// Check if service exists
// ================================

auto ServiceRegistry::has_service(const std::string& service_name) const -> bool {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = services_.find(service_name);
    if (it == services_.end()) {
        return false;
    }

    return !it->second.empty();
}

// ================================
// Get all registered service names
// ================================

auto ServiceRegistry::get_all_services() const -> std::vector<std::string> {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<std::string> services;
    services.reserve(services_.size());

    for (const auto& [service_name, queues] : services_) {
        if (!queues.empty()) {
            services.push_back(service_name);
        }
    }

    return services;
}

// ================================
// Subscribe to topic
// ================================

auto ServiceRegistry::subscribe(
    const std::string& queue_id,
    const std::string& topic
) -> void {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Check if queue is registered
    if (!queue_to_service_.contains(queue_id)) {
        logger_->warn("Queue {} not registered, cannot subscribe to topic {}", queue_id, topic);
        return;
    }

    // Check if already subscribed
    auto& subscribers = subscriptions_[topic];
    auto it = std::find(subscribers.begin(), subscribers.end(), queue_id);
    if (it != subscribers.end()) {
        logger_->debug("Queue {} already subscribed to topic {} (idempotent operation)", queue_id, topic);
        return;
    }

    // Add subscription
    subscribers.push_back(queue_id);

    logger_->info("Queue {} subscribed to topic {} (topic subscriber count: {})",
        queue_id, topic, subscribers.size());
}

// ================================
// Unsubscribe
// ================================

auto ServiceRegistry::unsubscribe(
    const std::string& queue_id,
    const std::string& topic
) -> void {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = subscriptions_.find(topic);
    if (it == subscriptions_.end()) {
        logger_->debug("Topic {} has no subscribers", topic);
        return;
    }

    auto& subscribers = it->second;
    auto sub_it = std::find(subscribers.begin(), subscribers.end(), queue_id);
    if (sub_it == subscribers.end()) {
        logger_->debug("Queue {} not subscribed to topic {}", queue_id, topic);
        return;
    }

    subscribers.erase(sub_it);

    // If no subscribers left, remove topic
    if (subscribers.empty()) {
        subscriptions_.erase(it);
        logger_->info("Topic {} has no subscribers, removing topic", topic);
    } else {
        logger_->info("Queue {} unsubscribed from topic {} (remaining subscribers: {})",
            queue_id, topic, subscribers.size());
    }
}

// ================================
// Get all subscribers for a topic
// ================================

auto ServiceRegistry::get_subscribers(const std::string& topic) const
    -> std::vector<std::string> {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = subscriptions_.find(topic);
    if (it == subscriptions_.end()) {
        return {};
    }

    return it->second;
}

// ================================
// Check if queue is subscribed to topic
// ================================

auto ServiceRegistry::is_subscribed(
    const std::string& queue_id,
    const std::string& topic
) const -> bool {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = subscriptions_.find(topic);
    if (it == subscriptions_.end()) {
        return false;
    }

    const auto& subscribers = it->second;
    return std::find(subscribers.begin(), subscribers.end(), queue_id)
        != subscribers.end();
}

// ================================
// Get service that queue belongs to
// ================================

auto ServiceRegistry::get_queue_service(const std::string& queue_id) const
    -> std::string {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = queue_to_service_.find(queue_id);
    if (it == queue_to_service_.end()) {
        return "";
    }

    return it->second;
}

// ================================
// Get statistics
// ================================

auto ServiceRegistry::get_stats() const -> RegistryStats {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    RegistryStats stats;

    stats.total_services = services_.size();
    stats.total_queues = queue_to_service_.size();

    // Calculate total subscriptions
    for (const auto& [topic, subscribers] : subscriptions_) {
        stats.total_subscriptions += subscribers.size();
    }

    return stats;
}

// ================================
// Clear all registration information
// ================================

auto ServiceRegistry::clear() -> void {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    size_t service_count = services_.size();
    size_t queue_count = queue_to_service_.size();
    size_t subscription_count = subscriptions_.size();

    services_.clear();
    subscriptions_.clear();
    queue_to_service_.clear();

    logger_->info("Service registry cleared (services: {}, queues: {}, topics: {})",
        service_count, queue_count, subscription_count);
}

} // namespace moltcat::messaging
