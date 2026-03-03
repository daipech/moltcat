#include "message_router.hpp"

namespace moltcat::messaging {

// ================================
// Constructor
// ================================

MessageRouter::MessageRouter(
    ServiceRegistry& registry,
    LoadBalanceStrategy lb_strategy
)
    : registry_(registry)
    , load_balancer_(std::make_unique<LoadBalancer>(lb_strategy))
    , logger_(spdlog::get("moltcat"))
{
    if (!logger_) {
        logger_ = spdlog::default_logger();
    }
    logger_->info("Message router initialized");
}

// ================================
// Route message (automatically select routing method)
// ================================

auto MessageRouter::route(
    const Message& msg,
    const std::string& sender_id,
    SendFunction send_func
) -> RoutingResult {
    // Check if message has destination address
    if (msg.header.destination.empty()) {
        return {
            .success = false,
            .recipients_count = 0,
            .error_message = "Message has no destination address"
        };
    }

    // Check if it's a special service name (indicates topic subscription)
    // Topics start with "topic:" prefix
    if (msg.header.destination.find("topic:") == 0) {
        std::string topic = msg.header.destination.substr(6); // Remove "topic:" prefix
        return publish_to_topic(msg, topic, sender_id, send_func);
    }

    // Default to point-to-point routing
    return route_point_to_point(msg, sender_id, send_func);
}

// ================================
// Point-to-point routing
// ================================

auto MessageRouter::route_point_to_point(
    const Message& msg,
    const std::string& sender_id,
    SendFunction send_func
) -> RoutingResult {
    const std::string& service_name = msg.header.destination;

    // Get all queues for the service
    auto queues = registry_.get_service_queues(service_name);

    if (queues.empty()) {
        logger_->warn("Service {} has no available instances", service_name);
        return {
            .success = false,
            .recipients_count = 0,
            .error_message = "Service " + service_name + " has no available instances"
        };
    }

    // Use load balancer to select a queue
    std::string selected_queue = load_balancer_->select_queue(queues);

    if (selected_queue.empty()) {
        return {
            .success = false,
            .recipients_count = 0,
            .error_message = "Load balancer did not select a queue"
        };
    }

    // Check if sending to self
    if (selected_queue == sender_id) {
        logger_->debug("Skip sending message to self: {}", msg.header.message_id);
        return {
            .success = true,
            .recipients_count = 0,
            .error_message = ""
        };
    }

    // Send message
    bool sent = send_func(selected_queue, msg);

    if (sent) {
        logger_->debug("Message {} routed to {} (service: {})",
            msg.header.message_id, selected_queue, service_name);
        return {
            .success = true,
            .recipients_count = 1,
            .error_message = ""
        };
    } else {
        logger_->error("Failed to send message to {}", selected_queue);
        return {
            .success = false,
            .recipients_count = 0,
            .error_message = "Failed to send to queue " + selected_queue
        };
    }
}

// ================================
// Broadcast
// ================================

auto MessageRouter::broadcast(
    const Message& msg,
    const std::string& sender_id,
    SendFunction send_func
) -> RoutingResult {
    const std::string& service_name = msg.header.destination;

    // Get all queues for the service
    auto queues = registry_.get_service_queues(service_name);

    if (queues.empty()) {
        logger_->warn("Service {} has no available instances, cannot broadcast", service_name);
        return {
            .success = false,
            .recipients_count = 0,
            .error_message = "Service " + service_name + " has no available instances"
        };
    }

    size_t success_count = 0;

    // Send to all queues (except sender itself)
    for (const auto& queue_id : queues) {
        // Skip sender itself
        if (queue_id == sender_id) {
            continue;
        }

        bool sent = send_func(queue_id, msg);
        if (sent) {
            success_count++;
        }
    }

    logger_->info("Broadcast message {} to service {}, success: {}/{}",
        msg.header.message_id, service_name, success_count, queues.size());

    return {
        .success = success_count > 0,
        .recipients_count = success_count,
        .error_message = success_count == 0 ? "All queues failed to send" : ""
    };
}

// ================================
// Publish to topic
// ================================

auto MessageRouter::publish_to_topic(
    const Message& msg,
    const std::string& topic,
    const std::string& sender_id,
    SendFunction send_func
) -> RoutingResult {
    // Get all subscribers for the topic
    auto subscribers = registry_.get_subscribers(topic);

    if (subscribers.empty()) {
        logger_->debug("Topic {} has no subscribers", topic);
        return {
            .success = true,  // No subscribers is not considered a failure
            .recipients_count = 0,
            .error_message = ""
        };
    }

    size_t success_count = 0;

    // Send to all subscribers (except sender itself)
    for (const auto& subscriber_id : subscribers) {
        // Skip sender itself
        if (subscriber_id == sender_id) {
            continue;
        }

        bool sent = send_func(subscriber_id, msg);
        if (sent) {
            success_count++;
        }
    }

    logger_->info("Published message {} to topic {}, success: {}/{}",
        msg.header.message_id, topic, success_count, subscribers.size());

    return {
        .success = true,  // Partial success is still considered success
        .recipients_count = success_count,
        .error_message = ""
    };
}

// ================================
// Set load balancing strategy
// ================================

auto MessageRouter::set_load_balance_strategy(LoadBalanceStrategy strategy)
    -> void {
    load_balancer_->set_strategy(strategy);
}

} // namespace moltcat::messaging
