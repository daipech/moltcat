#pragma once

#include "../protocol/a2a_types.hpp"
#include <zmq.hpp>
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace moltcat::messaging {

// 使用 A2A 协议的 Message 类型
using A2AMessage = moltcat::protocol::a2a::Message;
using A2ATask = moltcat::protocol::a2a::Task;
using A2ARole = moltcat::protocol::a2a::Role;
using A2ATaskState = moltcat::protocol::a2a::TaskState;

// ================================
// ZeroMQ Socket Type Encapsulation
// ================================

/**
 * @brief ZeroMQ socket type enumeration
 *
 * Corresponds to various ZeroMQ socket patterns
 */
enum class QueueType : int {
    PUB = ZMQ_PUB,         // Publisher (one-way broadcast)
    SUB = ZMQ_SUB,         // Subscriber (receive broadcast)
    REQ = ZMQ_REQ,         // Requester (synchronous request-response)
    REP = ZMQ_REP,         // Responder (synchronous response-request)
    DEALER = ZMQ_DEALER,   // Dealer (asynchronous request-response)
    ROUTER = ZMQ_ROUTER,   // Router (asynchronous routing)
    PUSH = ZMQ_PUSH,       // Pusher (pipeline downstream)
    PULL = ZMQ_PULL,       // Puller (pipeline upstream)
    PAIR = ZMQ_PAIR        // Bidirectional pairing
};

/**
 * @brief Convert QueueType to string (for debugging)
 */
inline auto queue_type_to_string(QueueType type) -> std::string_view {
    switch (type) {
        case QueueType::PUB:    return "PUB";
        case QueueType::SUB:    return "SUB";
        case QueueType::REQ:    return "REQ";
        case QueueType::REP:    return "REP";
        case QueueType::DEALER: return "DEALER";
        case QueueType::ROUTER: return "ROUTER";
        case QueueType::PUSH:   return "PUSH";
        case QueueType::PULL:   return "PULL";
        case QueueType::PAIR:   return "PAIR";
        default:                return "UNKNOWN";
    }
};

// ================================
// Message Queue Interface
// ================================

/**
 * @brief Message queue abstract interface
 *
 * Defines behavior that all message queue implementations must follow
 * Now based on A2A protocol message types
 */
class IMessageQueue {
public:
    virtual ~IMessageQueue() = default;

    /**
     * @brief Bind to local endpoint (as server)
     *
     * @param endpoint Endpoint address (format: tcp://*:5555 or ipc:///tmp/moltcat.ipc)
     * @return true indicates success
     */
    virtual auto bind(const std::string& endpoint) -> bool = 0;

    /**
     * @brief Connect to remote endpoint (as client)
     *
     * @param endpoint Endpoint address (format: tcp://localhost:5555)
     * @return true indicates success
     */
    virtual auto connect(const std::string& endpoint) -> bool = 0;

    /**
     * @brief Send A2A message
     *
     * @param msg A2A message
     * @param timeout_ms Timeout in milliseconds, 0 means non-blocking, -1 means blocking forever
     * @return true indicates successful send
     */
    virtual auto send(const A2AMessage& msg, int timeout_ms = -1) -> bool = 0;

    /**
     * @brief Receive A2A message
     *
     * @param timeout_ms Timeout in milliseconds, 0 means non-blocking, -1 means blocking forever
     * @return Received message, returns std::nullopt on timeout or failure
     */
    virtual auto receive(int timeout_ms = -1) -> std::optional<A2AMessage> = 0;

    /**
     * @brief Bulk send A2A messages
     *
     * @param messages Message list
     * @param timeout_ms Timeout
     * @return Number of messages successfully sent
     */
    virtual auto send_bulk(
        const std::vector<A2AMessage>& messages,
        int timeout_ms = -1
    ) -> size_t = 0;

    /**
     * @brief Bulk receive A2A messages (up to max_count)
     *
     * @param max_count Maximum receive count
     * @param timeout_ms Timeout
     * @return Received message list
     */
    virtual auto receive_bulk(
        size_t max_count,
        int timeout_ms = -1
    ) -> std::vector<A2AMessage> = 0;

    /**
     * @brief Subscribe to topic (only valid for SUB type)
     *
     * @param topic Topic string (empty string means subscribe to all)
     */
    virtual auto subscribe(const std::string& topic) -> void = 0;

    /**
     * @brief Unsubscribe (only valid for SUB type)
     *
     * @param topic Topic string
     */
    virtual auto unsubscribe(const std::string& topic) -> void = 0;

    /**
     * @brief Start queue (begin processing messages)
     */
    virtual auto start() -> void = 0;

    /**
     * @brief Stop queue (graceful shutdown)
     */
    virtual auto stop() -> void = 0;

    /**
     * @brief Check if queue is running
     */
    virtual auto is_running() const -> bool = 0;

    /**
     * @brief Get queue type
     */
    virtual auto get_type() const -> QueueType = 0;

    /**
     * @brief Get statistics
     */
    struct Stats {
        uint64_t messages_sent = 0;
        uint64_t messages_received = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t send_errors = 0;
        uint64_t receive_errors = 0;
    };

    virtual auto get_stats() const -> Stats = 0;

    /**
     * @brief Reset statistics
     */
    virtual auto reset_stats() -> void = 0;
};

// ================================
// ZeroMQ Queue Configuration
// ================================

/**
 * @brief ZeroMQ queue configuration options
 */
struct ZeroMQQueueConfig {
    // Socket high water mark (send buffer size)
    int send_hwm = 1000;

    // Socket high water mark (receive buffer size)
    int receive_hwm = 1000;

    // TCP keepalive enabled
    bool tcp_keepalive = true;

    // TCP keepalive idle time (seconds)
    int tcp_keepalive_idle = 30;

    // TCP keepalive interval (seconds)
    int tcp_keepalive_interval = 5;

    // I/O thread count
    int io_threads = 1;

    // Linger time (wait time on close, milliseconds)
    int linger = 1000;

    // Enable IPv6
    bool ipv6_enabled = false;

    // Zero-copy optimization (Phase 2 addition)
    bool enable_zero_copy = true;

    // Message compression threshold (bytes, auto compress if exceeded)
    size_t compression_threshold = 1024 * 10;  // 10KB

    // Enable message compression
    bool enable_compression = false;

    /**
     * @brief Validate configuration validity
     */
    auto validate() const -> bool {
        if (send_hwm < 0) return false;
        if (receive_hwm < 0) return false;
        if (io_threads < 1) return false;
        return true;
    }
};

} // namespace moltcat::messaging
