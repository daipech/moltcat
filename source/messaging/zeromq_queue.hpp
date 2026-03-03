#pragma once

#include "message_queue.hpp"
#include "../protocol/a2a_types.hpp"
#include <zmq.hpp>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <spdlog/spdlog.h>

namespace moltcat::messaging {

// ================================
// ZeroMQ Message Queue Implementation (A2A Version)
// ================================

/**
 * @brief ZeroMQ message queue implementation
 *
 * Based on A2A protocol message types, supports zero-copy optimization
 */
class ZeroMQMessageQueue : public IMessageQueue {
public:
    /**
     * @brief Constructor
     *
     * @param type Queue type
     * @param context ZeroMQ context (shared)
     * @param config Configuration options
     */
    ZeroMQMessageQueue(
        QueueType type,
        zmq::context_t& context,
        const ZeroMQQueueConfig& config = {}
    );

    ~ZeroMQMessageQueue() override;

    // Disable copy and move
    ZeroMQMessageQueue(const ZeroMQMessageQueue&) = delete;
    ZeroMQMessageQueue& operator=(const ZeroMQMessageQueue&) = delete;
    ZeroMQMessageQueue(ZeroMQMessageQueue&&) = delete;
    ZeroMQMessageQueue& operator=(ZeroMQMessageQueue&&) = delete;

    // Implement IMessageQueue interface
    auto bind(const std::string& endpoint) -> bool override;
    auto connect(const std::string& endpoint) -> bool override;
    auto send(const A2AMessage& msg, int timeout_ms = -1) -> bool override;
    auto receive(int timeout_ms = -1) -> std::optional<A2AMessage> override;
    auto send_bulk(const std::vector<A2AMessage>& messages, int timeout_ms = -1) -> size_t override;
    auto receive_bulk(size_t max_count, int timeout_ms = -1) -> std::vector<A2AMessage> override;
    auto subscribe(const std::string& topic) -> void override;
    auto unsubscribe(const std::string& topic) -> void override;
    auto start() -> void override;
    auto stop() -> void override;
    auto is_running() const -> bool override;
    auto get_type() const -> QueueType override;
    auto get_stats() const -> Stats override;
    auto reset_stats() -> void override;

private:
    // ZeroMQ socket
    std::unique_ptr<zmq::socket_t> socket_;

    // ZeroMQ context reference
    zmq::context_t& context_;

    // Queue type
    QueueType type_;

    // Configuration
    ZeroMQQueueConfig config_;

    // Running state
    std::atomic<bool> running_{false};

    // Statistics (thread-safe)
    mutable std::mutex stats_mutex_;
    Stats stats_;

    // Logger
    std::shared_ptr<spdlog::logger> logger_;

    // Helper method: apply socket options
    auto apply_socket_options() -> void;

    // Helper method: serialize message (supports zero-copy)
    auto serialize_message(const A2AMessage& msg) -> std::string;

    // Helper method: deserialize message
    auto deserialize_message(const std::string& data) -> std::optional<A2AMessage>;

    // Helper method: zero-copy send (Phase 2 optimization)
    auto send_zero_copy(const A2AMessage& msg, int timeout_ms) -> bool;

    // Helper method: update statistics
    auto update_send_stats(size_t bytes) -> void;
    auto update_receive_stats(size_t bytes) -> void;
};

// ================================
// Constructor
// ================================

inline ZeroMQMessageQueue::ZeroMQMessageQueue(
    QueueType type,
    zmq::context_t& context,
    const ZeroMQQueueConfig& config
)
    : context_(context)
    , type_(type)
    , config_(config)
    , logger_(spdlog::get("moltcat"))
{
    // Validate configuration
    if (!config_.validate()) {
        throw std::invalid_argument("Invalid ZeroMQ queue configuration");
    }

    // Create socket
    socket_ = std::make_unique<zmq::socket_t>(context_, static_cast<int>(type));

    // Apply socket options
    apply_socket_options();

    logger_->info("ZeroMQ queue created successfully, type: {}", queue_type_to_string(type));
}

// ================================
// Destructor
// ================================

inline ZeroMQMessageQueue::~ZeroMQMessageQueue() {
    stop();
}

// ================================
// Apply socket options
// ================================

inline auto ZeroMQMessageQueue::apply_socket_options() -> void {
    // Set high water mark
    socket_->set(zmq::sockopt::sndhwm, config_.send_hwm);
    socket_->set(zmq::sockopt::rcvhwm, config_.receive_hwm);

    // Set TCP keepalive
    socket_->set(zmq::sockopt::tcp_keepalive, config_.tcp_keepalive ? 1 : 0);
    socket_->set(zmq::sockopt::tcp_keepalive_idle, config_.tcp_keepalive_idle);
    socket_->set(zmq::sockopt::tcp_keepalive_intvl, config_.tcp_keepalive_interval);

    // Set linger time
    socket_->set(zmq::sockopt::linger, config_.linger);

    // Enable IPv6 (if configured)
    if (config_.ipv6_enabled) {
        socket_->set(zmq::sockopt::ipv6, true);
    }

    logger_->debug("ZeroMQ socket options applied, zero-copy: {}", config_.enable_zero_copy);
}

// ================================
// Bind endpoint
// ================================

inline auto ZeroMQMessageQueue::bind(const std::string& endpoint) -> bool {
    try {
        socket_->bind(endpoint);
        logger_->info("ZeroMQ queue bound successfully: {}", endpoint);
        return true;
    } catch (const zmq::error_t& e) {
        logger_->error("ZeroMQ queue bind failed: {}, error: {}", endpoint, e.what());
        return false;
    }
}

// ================================
// Connect endpoint
// ================================

inline auto ZeroMQMessageQueue::connect(const std::string& endpoint) -> bool {
    try {
        socket_->connect(endpoint);
        logger_->info("ZeroMQ queue connected successfully: {}", endpoint);
        return true;
    } catch (const zmq::error_t& e) {
        logger_->error("ZeroMQ queue connection failed: {}, error: {}", endpoint, e.what());
        return false;
    }
}

// ================================
// Serialize message
// ================================

inline auto ZeroMQMessageQueue::serialize_message(const A2AMessage& msg) -> std::string {
    // Use Glaze serialization (pure JSON, per decision point 1)
    std::string json;
    auto error = glz::write_json(msg, json);
    if (error) {
        throw std::runtime_error("Message serialization failed: " + std::string(error));
    }
    return json;
}

// ================================
// Deserialize message
// ================================

inline auto ZeroMQMessageQueue::deserialize_message(
    const std::string& data
) -> std::optional<A2AMessage> {
    return A2AMessage::deserialize(data);
}

// ================================
// Zero-copy send (Phase 2 optimization)
// ================================

inline auto ZeroMQMessageQueue::send_zero_copy(const A2AMessage& msg, int timeout_ms) -> bool {
    try {
        // 1. Serialize message
        auto json_data = serialize_message(msg);

        // 2. If zero-copy enabled and large data, use buffer_init
        if (config_.enable_zero_copy && json_data.size() > 1024) {
            // Zero-copy method: use const char* directly, avoid copying
            zmq::message_t zmq_msg(
                json_data.data(),
                json_data.size(),
                [](void* data, void* hint) {
                    // Empty destructor (ZeroMQ will call automatically after sending)
                    // Data lifecycle managed by caller
                }
            );

            // Set send timeout
            if (timeout_ms >= 0) {
                socket_->set(zmq::sockopt::sndtimeo, timeout_ms);
            }

            // Send
            auto result = socket_->send(zmq_msg, zmq::send_flags::none);

            if (result) {
                update_send_stats(json_data.size());
                return true;
            } else {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.send_errors++;
                return false;
            }
        } else {
            // Traditional method: copy data
            zmq::message_t zmq_msg(json_data.begin(), json_data.end());

            if (timeout_ms >= 0) {
                socket_->set(zmq::sockopt::sndtimeo, timeout_ms);
            }

            auto result = socket_->send(zmq_msg, zmq::send_flags::none);

            if (result) {
                update_send_stats(json_data.size());
                return true;
            } else {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.send_errors++;
                return false;
            }
        }

    } catch (const zmq::error_t& e) {
        logger_->error("Message send exception: {}", e.what());
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.send_errors++;
        return false;
    }
}

// ================================
// Send message
// ================================

inline auto ZeroMQMessageQueue::send(const A2AMessage& msg, int timeout_ms) -> bool {
    // Use zero-copy optimization
    return send_zero_copy(msg, timeout_ms);
}

// ================================
// Receive message
// ================================

inline auto ZeroMQMessageQueue::receive(int timeout_ms) -> std::optional<A2AMessage> {
    try {
        // Set receive timeout
        if (timeout_ms >= 0) {
            socket_->set(zmq::sockopt::rcvtimeo, timeout_ms);
        }

        // Receive message
        zmq::message_t zmq_msg;
        auto result = socket_->recv(zmq_msg, zmq::recv_flags::none);

        if (!result) {
            // Timeout or no message
            return std::nullopt;
        }

        // Extract data
        std::string json_data;
        json_data.resize(zmq_msg.size());
        std::memcpy(json_data.data(), zmq_msg.data(), zmq_msg.size());

        // Deserialize
        auto msg = deserialize_message(json_data);

        if (msg) {
            update_receive_stats(json_data.size());
            logger_->debug("Message received successfully, ID: {}, size: {} bytes",
                msg->message_id, json_data.size());
        } else {
            logger_->error("Message deserialization failed");
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.receive_errors++;
        }

        return msg;

    } catch (const zmq::error_t& e) {
        logger_->error("Message receive exception: {}", e.what());
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.receive_errors++;
        return std::nullopt;
    }
}

// ================================
// Bulk send
// ================================

inline auto ZeroMQMessageQueue::send_bulk(
    const std::vector<A2AMessage>& messages,
    int timeout_ms
) -> size_t {
    size_t success_count = 0;

    for (const auto& msg : messages) {
        if (send(msg, timeout_ms)) {
            success_count++;
        }
    }

    logger_->debug("Bulk send completed, success: {}/{}", success_count, messages.size());
    return success_count;
}

// ================================
// Bulk receive
// ================================

inline auto ZeroMQMessageQueue::receive_bulk(
    size_t max_count,
    int timeout_ms
) -> std::vector<A2AMessage> {
    std::vector<A2AMessage> messages;
    messages.reserve(max_count);

    // Set shorter single timeout to quickly collect multiple messages
    int single_timeout = timeout_ms > 0 ? timeout_ms / static_cast<int>(max_count) : 100;

    for (size_t i = 0; i < max_count; ++i) {
        auto msg = receive(single_timeout);
        if (msg) {
            messages.push_back(std::move(*msg));
        } else {
            // No more messages
            break;
        }
    }

    logger_->debug("Bulk receive completed, received: {}", messages.size());
    return messages;
}

// ================================
// Subscribe to topic
// ================================

inline auto ZeroMQMessageQueue::subscribe(const std::string& topic) -> void {
    try {
        socket_->set(zmq::sockopt::subscribe, topic);
        logger_->info("Subscribed to topic: {}", topic.empty() ? "(all)" : topic);
    } catch (const zmq::error_t& e) {
        logger_->error("Subscription failed: {}", e.what());
    }
}

// ================================
// Unsubscribe
// ================================

inline auto ZeroMQMessageQueue::unsubscribe(const std::string& topic) -> void {
    try {
        socket_->set(zmq::sockopt::unsubscribe, topic);
        logger_->info("Unsubscribed from topic: {}", topic.empty() ? "(all)" : topic);
    } catch (const zmq::error_t& e) {
        logger_->error("Unsubscription failed: {}", e.what());
    }
}

// ================================
// Start queue
// ================================

inline auto ZeroMQMessageQueue::start() -> void {
    running_ = true;
    logger_->info("ZeroMQ queue started, type: {}", queue_type_to_string(type_));
}

// ================================
// Stop queue
// ================================

inline auto ZeroMQMessageQueue::stop() -> void {
    if (running_.exchange(false)) {
        logger_->info("ZeroMQ queue shutting down...");

        // Close socket
        if (socket_) {
            socket_->close();
        }

        logger_->info("ZeroMQ queue shut down");
    }
}

// ================================
// Check running status
// ================================

inline auto ZeroMQMessageQueue::is_running() const -> bool {
    return running_.load();
}

// ================================
// Get queue type
// ================================

inline auto ZeroMQMessageQueue::get_type() const -> QueueType {
    return type_;
}

// ================================
// Get statistics
// ================================

inline auto ZeroMQMessageQueue::get_stats() const -> Stats {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

// ================================
// Reset statistics
// ================================

inline auto ZeroMQMessageQueue::reset_stats() -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = Stats{};
    logger_->info("Statistics reset");
}

// ================================
// Update send statistics
// ================================

inline auto ZeroMQMessageQueue::update_send_stats(size_t bytes) -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.messages_sent++;
    stats_.bytes_sent += bytes;
}

// ================================
// Update receive statistics
// ================================

inline auto ZeroMQMessageQueue::update_receive_stats(size_t bytes) -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.messages_received++;
    stats_.bytes_received += bytes;
}

} // namespace moltcat::messaging
