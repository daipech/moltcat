#pragma once

/**
 * @file message_bus.hpp
 * @brief Unified message bus interface
 *
 * This header file provides a backward-compatible message bus interface, internally uses the new MessageBusClient implementation.
 *
 * Migration guide:
 * - Old architecture: shared memory + queue management
 * - New architecture: Server-Client (ZeroMQ) + service registration
 *
 * Old code migration:
 * ```cpp
 * // Old code
 * auto bus = std::make_shared<MessageBus>();
 * bus->start();
 * bus->create_queue("agent_team", QueueType::PAIR);
 *
 * // New code
 * MessageBusClient::Config config;
 * config.server_endpoint = "ipc:///tmp/moltcat_messagebus.sock";
 * config.service_name = "MyService";
 * MessageBusClient bus(config);
 * bus.connect();
 * ```
 *
 * @see message_bus_client.hpp - New client implementation
 * @see message_bus_server.hpp - Server implementation
 */

#include "message_bus_client.hpp"
#include "message.hpp"
#include <string>
#include <functional>
#include <memory>

namespace moltcat::messaging {

// ================================
// Configuration Structure (Backward Compatible)
// ================================

/**
 * @brief Message bus configuration (deprecated)
 *
 * @deprecated This configuration structure is kept only for backward compatibility, please use MessageBusClient::Config directly
 */
struct [[deprecated("Please use MessageBusClient::Config")]] MessageBusConfig {
    // ZeroMQ I/O threads (deprecated)
    int io_threads = 1;

    // Maximum message queue size (deprecated)
    size_t max_queue_size = 10000;

    // Message handler threads (deprecated)
    size_t worker_threads = 4;

    // Server endpoint (used in new architecture)
    std::string server_endpoint = "ipc:///tmp/moltcat_messagebus.sock";

    // Service name (used in new architecture)
    std::string service_name = "UnknownService";

    // Enable message tracing (deprecated)
    bool enable_message_tracing = false;
};

// ================================
// Unified Message Bus Interface
// ================================

/**
 * @brief Message bus (adapter class)
 *
 * This class provides a backward-compatible message bus interface, internally delegates to MessageBusClient.
 *
 * Important notes:
 * - No longer supports queue management (create_queue, remove_queue etc. removed)
 * - No longer supports handler registration (register_handler etc. removed)
 * - Service registration and discovery automatically managed by MessageBusServer
 * - Recommend using MessageBusClient directly for full functionality
 *
 * @deprecated Recommend using MessageBusClient or MessageBusServer directly
 */
class [[deprecated("Recommend using MessageBusClient or MessageBusServer directly")]] MessageBus {
public:
    /**
     * @brief Constructor
     *
     * @param config Configuration (only some fields are valid)
     *
     * Valid configuration fields:
     * - server_endpoint: server address
     * - service_name: service name
     *
     * Invalid configuration fields (deprecated):
     * - io_threads, max_queue_size, worker_threads etc.
     */
    explicit MessageBus(const MessageBusConfig& config = {});

    /**
     * @brief Destructor
     *
     * Automatically disconnects
     */
    ~MessageBus();

    // Disable copy and move
    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;
    MessageBus(MessageBus&&) = delete;
    MessageBus& operator=(MessageBus&&) = delete;

    // ====================
    // Connection Management
    // ====================

    /**
     * @brief Start message bus (connect to server)
     *
     * @return true indicates successful connection
     *
     * Note: This method connects to MessageBusServer, will fail if server is not started
     */
    auto start() -> bool;

    /**
     * @brief Stop message bus (disconnect)
     */
    auto stop() -> void;

    // ====================
    // Message Sending API (Retained)
    // ====================

    /**
     * @brief Send message (auto routing)
     *
     * @param msg Message
     * @param timeout_ms Timeout in milliseconds
     * @return true indicates successful send
     *
     * Message will be automatically routed to destination service based on msg.header.destination
     */
    auto send(const Message& msg, int timeout_ms = -1) -> bool;

    /**
     * @brief Send message to specified service
     *
     * @param service_name Service name
     * @param msg Message
     * @param timeout_ms Timeout in milliseconds
     * @return true indicates successful send
     *
     * This method automatically sets msg.header.destination = service_name
     */
    auto send_to(
        const std::string& service_name,
        const Message& msg,
        int timeout_ms = -1
    ) -> bool;

    /**
     * @brief Broadcast message (send to all instances of a service)
     *
     * @param msg Message
     * @param timeout_ms Timeout in milliseconds
     * @return Number of instances successfully sent to
     *
     * Note: msg.header.destination must specify target service
     */
    auto broadcast(const Message& msg, int timeout_ms = -1) -> size_t;

    // ====================
    // Subscription API (Retained)
    // ====================

    /**
     * @brief Subscribe to topic
     *
     * @param subscriber_queue Subscriber identifier (deprecated, automatically uses current client)
     * @param topic Topic name
     *
     * @deprecated This method signature is outdated, please use MessageBusClient::subscribe() directly
     * New version: client.subscribe(topic);
     */
    [[deprecated("Please use MessageBusClient::subscribe(const std::string&)")]]
    auto subscribe(const std::string& subscriber_queue, const std::string& topic) -> void;

    /**
     * @brief Start message callback (asynchronous receiving)
     *
     * @param callback Message processing callback
     *
     * Note: This is the recommended receiving method in the new architecture
     */
    auto start_message_loop(std::function<void(const Message&)> callback) -> void;

    /**
     * @brief Stop message loop
     */
    auto stop_message_loop() -> void;

    // ====================
    // Removed APIs
    // ====================

    /**
     * @brief Create queue (removed)
     *
     * @deprecated In the new architecture, queues are automatically managed by MessageBusServer, no manual creation needed
     *
     * Migration guide:
     * - Remove create_queue calls
     * - Service automatically registers to server on startup
     * - Server automatically creates message queues for each service
     */
    [[deprecated("Removed: queues automatically managed by server")]]
    auto create_queue(
        const std::string& name,
        QueueType type,
        const ZeroMQQueueConfig& config = {}
    ) -> IMessageQueue* = delete;

    /**
     * @brief Register message handler (removed)
     *
     * @deprecated In the new architecture, use start_message_loop to set message callback
     *
     * Migration guide:
     * ```cpp
     * // Old code
     * class MyHandler : public IMessageHandler {
     *     auto handle_message(const Message& msg) -> std::optional<Message> override;
     * };
     * bus->register_handler(std::make_shared<MyHandler>());
     *
     * // New code
     * bus->start_message_loop([](const Message& msg) {
     *     // Handle message
     * });
     * ```
     */
    [[deprecated("Removed: please use start_message_loop to set callback")]]
    auto register_handler(
        std::shared_ptr<IMessageHandler> handler,
        const std::string& queue_name = ""
    ) -> void = delete;

    /**
     * @brief 移除队列（已移除）
     *
     * @deprecated 队列由服务器自动管理
     */
    [[deprecated("已移除：队列由服务器自动管理")]]
    auto remove_queue(const std::string& name) -> bool = delete;

    /**
     * @brief 取消注册处理器（已移除）
     *
     * @deprecated 请使用 stop_message_loop
     */
    [[deprecated("已移除：请使用 stop_message_loop")]]
    auto unregister_handler(IMessageHandler* handler) -> void = delete;

    /**
     * @brief 获取队列（已移除）
     *
     * @deprecated 新架构中不直接暴露队列对象
     */
    [[deprecated("已移除：新架构中不直接暴露队列")]]
    auto get_queue(const std::string& name) -> IMessageQueue* = delete;

    // ====================
    // 获取底层客户端
    // ====================

    /**
     * @brief 获取底层的 MessageBusClient
     *
     * 用于访问新架构的完整功能
     *
     * @return MessageBusClient 指针
     */
    auto get_client() -> MessageBusClient* { return client_.get(); }

    /**
     * @brief 获取底层的 MessageBusClient（const 版本）
     *
     * @return MessageBusClient const 指针
     */
    auto get_client() const -> const MessageBusClient* { return client_.get(); }

private:
    // 底层客户端实现
    std::unique_ptr<MessageBusClient> client_;

    // 配置
    MessageBusConfig config_;
};

// ================================
// 内联实现
// ================================

inline MessageBus::MessageBus(const MessageBusConfig& config)
    : config_(config)
{
    // 创建底层客户端
    MessageBusClient::Config client_config;
    client_config.server_endpoint = config.server_endpoint;
    client_config.service_name = config.service_name;
    client_config.queue_id = "auto";  // 自动生成

    client_ = std::make_unique<MessageBusClient>(client_config);
}

inline MessageBus::~MessageBus() {
    if (client_ && client_->is_connected()) {
        client_->disconnect();
    }
}

inline auto MessageBus::start() -> bool {
    if (!client_) {
        return false;
    }
    return client_->connect();
}

inline auto MessageBus::stop() -> void {
    if (client_) {
        client_->stop_message_loop();
        client_->disconnect();
    }
}

inline auto MessageBus::send(const Message& msg, int timeout_ms) -> bool {
    if (!client_ || !client_->is_connected()) {
        return false;
    }
    return client_->send(msg, timeout_ms);
}

inline auto MessageBus::send_to(
    const std::string& service_name,
    const Message& msg,
    int timeout_ms
) -> bool {
    if (!client_ || !client_->is_connected()) {
        return false;
    }
    return client_->send_to(service_name, msg, timeout_ms);
}

inline auto MessageBus::broadcast(const Message& msg, int timeout_ms) -> size_t {
    if (!client_ || !client_->is_connected()) {
        return 0;
    }

    if (msg.header.destination.empty()) {
        return 0;
    }

    return client_->broadcast(msg.header.destination, msg, timeout_ms);
}

inline auto MessageBus::subscribe(
    const std::string& subscriber_queue,
    const std::string& topic
) -> void {
    if (client_) {
        // 忽略 subscriber_queue，使用当前客户端
        client_->subscribe(topic);
    }
}

inline auto MessageBus::start_message_loop(
    std::function<void(const Message&)> callback
) -> void {
    if (client_) {
        client_->start_message_loop(callback);
    }
}

inline auto MessageBus::stop_message_loop() -> void {
    if (client_) {
        client_->stop_message_loop();
    }
}

} // namespace moltcat::messaging
