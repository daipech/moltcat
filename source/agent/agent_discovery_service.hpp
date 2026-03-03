#pragma once

#include "messaging/message_handler.hpp"
#include "agent/team_registry.hpp"
#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace moltcat::agent {

/**
 * AgentDiscoveryService - Agent discovery service
 *
 * Responsibilities:
 * 1. Handle Agent registration requests (AGENT_REGISTER)
 * 2. Handle Agent deregistration requests (AGENT_UNREGISTER)
 * 3. Handle Team member queries (TEAM_DISCOVER_MEMBERS)
 * 4. Broadcast Team member change events (TEAM_MEMBER_JOINED / TEAM_MEMBER_LEFT)
 * 5. Maintain TeamRegistry (Agent -> Team mapping)
 *
 * Workflow:
 * 1. Agent sends AGENT_REGISTER message on startup
 * 2. Service registers Agent to TeamRegistry
 * 3. Service broadcasts TEAM_MEMBER_JOINED event
 * 4. Other Agents subscribe to events, update local member lists
 *
 * Message formats:
 * - AGENT_REGISTER: AgentRegisterRequest (JSON)
 * - AGENT_UNREGISTER: {agent_id: string}
 * - TEAM_DISCOVER_MEMBERS: {team_id: string}
 * - TEAM_MEMBERS_RESPONSE: TeamMembersResponse (JSON)
 * - TEAM_MEMBER_JOINED: AgentInfo (JSON)
 * - TEAM_MEMBER_LEFT: {agent_id, team_id} (JSON)
 */
class AgentDiscoveryService : public messaging::MessageHandlerBase {
public:
    // ================================
    // Constructors
    // ================================

    /**
     * Constructor
     *
     * @param registry Team registry (shared ownership)
     * @param logger Logger (optional)
     */
    explicit AgentDiscoveryService(
        std::shared_ptr<TeamRegistry> registry,
        std::shared_ptr<spdlog::logger> logger = nullptr
    );

    ~AgentDiscoveryService() override = default;

    // ================================
    // IMessageHandler Interface Implementation
    // ================================

    /**
     * Handle message
     *
     * @param msg Received message
     * @return Optional response message
     */
    auto handle_message(const messaging::Message& msg)
        -> std::optional<messaging::Message> override;

    /**
     * Check if can handle specific type of message
     *
     * @param type Message type
     * @return true indicates can handle
     */
    auto can_handle(messaging::MessageType type) const -> bool override;

    // ================================
    // Lifecycle Management
    // ================================

    /**
     * Start service
     *
     * @note Register handler on MessageBus
     * @note Create dedicated service queue
     */
    auto start() -> void;

    /**
     * Stop service
     *
     * @note Unregister handler from MessageBus
     */
    auto stop() -> void;

    /**
     * Get service status
     */
    auto is_running() const -> bool { return running_; }

    // ================================
    // Configuration
    // ================================

    /**
     * 设置服务队列名称
     *
     * @param name 队列名称（默认："agent_discovery_service"）
     */
    auto set_service_queue_name(const std::string& name) -> void {
        service_queue_name_ = name;
    }

    /**
     * 设置广播队列名称
     *
     * @param name 队列名称（默认："team_updates"）
     */
    auto set_broadcast_queue_name(const std::string& name) -> void {
        broadcast_queue_name_ = name;
    }

    /**
     * 设置 MessageBusClient 引用
     *
     * @param message_bus_client MessageBusClient 指针
     *
     * @note 用于广播成员变更事件
     */
    auto set_message_bus_client(messaging::MessageBusClient* message_bus_client) -> void {
        message_bus_client_ = message_bus_client;
    }

private:
    // ================================
    // 消息处理方法
    // ================================

    /**
     * 处理 Agent 注册请求
     */
    auto handle_register(const messaging::Message& msg)
        -> std::optional<messaging::Message>;

    /**
     * 处理 Agent 注销请求
     */
    auto handle_unregister(const messaging::Message& msg)
        -> std::optional<messaging::Message>;

    /**
     * 处理 Team 成员查询请求
     */
    auto handle_discover_members(const messaging::Message& msg)
        -> std::optional<messaging::Message>;

    /**
     * 广播成员加入事件
     */
    auto broadcast_member_joined(const messaging::AgentInfo& info) -> void;

    /**
     * 广播成员离开事件
     */
    auto broadcast_member_left(
        const std::string& agent_id,
        const std::string& team_id
    ) -> void;

    // ================================
    // 辅助方法
    // ================================

    /**
     * 创建成功响应消息
     */
    auto create_success_response(
        const std::string& original_message_id,
        const std::string& source
    ) -> messaging::Message;

    /**
     * 创建错误响应消息
     */
    auto create_error_response(
        const std::string& original_message_id,
        const std::string& source,
        const std::string& error_message
    ) -> messaging::Message;

    // ================================
    // 字段
    // ================================

    std::shared_ptr<TeamRegistry> registry_;       // Team 注册表
    std::shared_ptr<spdlog::logger> logger_;       // 日志记录器
    std::atomic<bool> running_{false};             // 运行状态

    // 队列名称
    std::string service_queue_name_ = "agent_discovery_service";
    std::string broadcast_queue_name_ = "team_updates";

    // MessageBusClient 指针（在 start() 时设置）
    messaging::MessageBusClient* message_bus_client_ = nullptr;
};

} // namespace moltcat::agent
