#pragma once

#include "agent.hpp"
#include "task_types.hpp"
#include "../messaging/message_queue.hpp"
#include "../messaging/message_bus_client.hpp"
#include "../messaging/context.hpp"
#include "../protocol/a2a_types.hpp"
#include "model/task.hpp"
#include "model/result.hpp"
#include "team_registry.hpp"
#include "agent_pool.hpp"
#include "utils/rate_limiter.hpp"
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <functional>

namespace moltcat::agent {

// A2A type aliases
using A2AMessage = moltcat::protocol::a2a::Message;
using A2ATask = moltcat::protocol::a2a::Task;
using A2ARole = moltcat::protocol::a2a::Role;
using A2ATaskState = moltcat::protocol::a2a::TaskState;
using A2ATaskStatus = moltcat::protocol::a2a::TaskStatus;

// ================================
// AgentTeam Message Types
// ================================

/**
 * @brief AgentTeam internal message types
 *
 * Used for communication between AgentTeam and Agents
 */
enum class TeamMessageType : uint8_t {
    TASK_REQUEST,      // Task request (AgentTeam → Agent)
    TASK_RESPONSE,     // Task response (Agent → AgentTeam)
    PIPELINE_FORWARD,  // Pipeline forward (AgentTeam → Agent, carries previous stage result)
    CANCEL_REQUEST,    // Cancel request (AgentTeam → Agent)
    HEARTBEAT          // Heartbeat (AgentTeam ↔ Agent)
};

/**
 * @brief AgentTeam message
 */
struct TeamMessage {
    TeamMessageType type;               // Message type
    std::string team_id;                 // Team ID
    std::string agent_id;                // Target/Source Agent ID
    std::string task_id;                 // Task ID
    A2ATask a2a_task;                    // A2A task
    std::string pipeline_data;           // Pipeline data (JSON format, previous stage result)

    /**
     * Convert to A2A Message (for sending via message queue)
     */
    [[nodiscard]] auto to_a2a_message() const -> A2AMessage {
        A2AMessage msg;
        msg.id = "team-" + team_id + "-" + task_id;
        msg.role = A2ARole::USER;
        msg.task = a2a_task;

        // Add metadata to task
        msg.task.metadata["team_id"] = team_id;
        msg.task.metadata["agent_id"] = agent_id;
        msg.task.metadata["message_type"] = static_cast<int>(type);
        msg.task.metadata["pipeline_data"] = pipeline_data;

        return msg;
    }

    /**
     * Parse from A2A Message
     */
    [[nodiscard]] static auto from_a2a_message(const A2AMessage& msg) -> TeamMessage {
        TeamMessage team_msg;
        team_msg.type = static_cast<TeamMessageType>(
            msg.task.metadata.value("message_type", static_cast<int>(TeamMessageType::TASK_REQUEST)).get<int>()
        );
        team_msg.team_id = msg.task.metadata.value("team_id", "").get<std::string>();
        team_msg.agent_id = msg.task.metadata.value("agent_id", "").get<std::string>();
        team_msg.task_id = msg.task.id;
        team_msg.a2a_task = msg.task;
        team_msg.pipeline_data = msg.task.metadata.value("pipeline_data", "").get<std::string>();

        return team_msg;
    }
};

// ================================
// AgentTeam (Decoupled Version)
// ================================

/**
 * @brief Agent team (decoupled version)
 *
 * **Architecture changes (Phase 3.3)**:
 * - ✅ Fully decoupled: only holds Agent IDs, not Agent pointers
 * - ✅ Message-based communication: communicates with Agents via ZeroMQ message queues
 * - ✅ Fully asynchronous: all execution methods are asynchronous, return immediately
 * - ✅ Context parameterization: uses new templated Context
 *
 * **Communication Architecture**:
 * ```
 * AgentTeam                           Agent 1
 *     │                                 │
 *     ├──[ZeroMQ PUB]───task─────────>│
 *     │                                 │
 *     │<──[ZeroMQ REP]───result───────│
 *     │                                 │
 *     ├──[ZeroMQ PUB]───task─────────>Agent 2
 *     │                                 │
 *     │<──[ZeroMQ REP]───result───────│
 *     │                                 │
 *     └──[ZeroMQ PUB]───task─────────>Agent N
 *                                       │
 *                                       │<──[ZeroMQ REP]───result───│
 * ```
 *
 * **Core Responsibilities**:
 * - Manage Agent ID list (not Agent instances)
 * - Distribute tasks to Agents via message queues
 * - Collect execution results from Agents
 * - Support four collaboration modes (pipeline, parallel, competitive, hierarchical)
 *
 * **Thread Safety**:
 * - All methods are thread-safe
 * - Uses mutex to protect internal state
 */
class AgentTeam {
public:
    /**
     * @brief Constructor (decoupled version)
     *
     * @param team_id Team ID
     * @param agent_ids Agent ID list (only holds IDs, not pointers)
     * @param mode Collaboration mode
     * @param message_queue Message queue (for communication with Agents)
     */
    AgentTeam(
        std::string team_id,
        std::vector<std::string> agent_ids,
        CollaborationMode mode,
        std::shared_ptr<messaging::IMessageQueue> message_queue
    );

    /**
     * @brief Destructor
     */
    ~AgentTeam();

    // Disable copy and move
    AgentTeam(const AgentTeam&) = delete;
    AgentTeam& operator=(const AgentTeam&) = delete;
    AgentTeam(AgentTeam&&) = delete;
    AgentTeam& operator=(AgentTeam&&) = delete;

    // ========== Team Management ==========

    /**
     * @brief Add Agent ID
     *
     * @param agent_id Agent ID
     */
    auto add_agent_id(std::string_view agent_id) -> void;

    /**
     * @brief Remove Agent ID
     *
     * @param agent_id Agent ID
     */
    auto remove_agent_id(std::string_view agent_id) -> void;

    /**
     * @brief Get all Agent IDs
     */
    [[nodiscard]] auto get_agent_ids() const -> const std::vector<std::string>&;

    /**
     * @brief Get Agent count
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    /**
     * @brief Check if Agent ID is in team
     */
    [[nodiscard]] auto has_agent_id(std::string_view agent_id) const -> bool;

    // ========== Asynchronous Task Execution ==========

    /**
     * @brief Pipeline asynchronous execution (PIPELINE mode)
     *
     * Agents process tasks sequentially, each Agent's output becomes the next Agent's input
     *
     * **Execution Flow**:
     * 1. Send first task to first Agent
     * 2. After receiving result, use result as next Agent's input
     * 3. Repeat until all Agents have processed
     * 4. If a stage fails, terminate pipeline
     *
     * @param tasks Task list (one task per Agent)
     * @param context Asynchronous context (uses a2a::Task as result type)
     */
    auto pipeline_execute_async(
        const std::vector<A2ATask>& tasks,
        std::shared_ptr<messaging::Context<std::vector<A2ATask>>> context
    ) -> void;

    /**
     * @brief Parallel asynchronous execution (PARALLEL mode)
     *
     * Multiple Agents process the same task simultaneously, return all results
     *
     * **Execution Flow**:
     * 1. Send task to all Agents simultaneously
     * 2. Asynchronously collect results from all Agents
     * 3. Set all results to Context after collection completes
     *
     * @param task Task
     * @param context Asynchronous context
     */
    auto parallel_execute_async(
        const A2ATask& task,
        std::shared_ptr<messaging::Context<std::vector<A2ATask>>> context
    ) -> void;

    /**
     * @brief Competitive asynchronous execution (COMPETITIVE mode)
     *
     * Multiple Agents process the same task simultaneously, using "first-come-first-served" strategy
     * The first successfully completed Agent's result is adopted, other Agents are cancelled
     *
     * **Execution Flow**:
     * 1. Send task to all Agents simultaneously
     * 2. Receive first successful result
     * 3. Cancel other Agents' tasks
     *
     * @param task Task
     * @param context Asynchronous context
     */
    auto competitive_execute_async(
        const A2ATask& task,
        std::shared_ptr<messaging::Context<A2ATask>> context
    ) -> void;

    /**
     * @brief Hierarchical asynchronous execution (HIERARCHICAL mode)
     *
     * Master Agent is responsible for task decomposition and coordination, worker Agents execute subtasks
     * Master Agent aggregates worker Agents' results and returns final result
     *
     * **Execution Flow**:
     * 1. Send task to master Agent
     * 2. Master Agent decomposes task into subtasks
     * 3. Worker Agents execute subtasks in parallel
     * 4. Master Agent aggregates results
     *
     * @param task Complex task
     * @param master_agent_id Master Agent ID
     * @param context Asynchronous context
     */
    auto hierarchical_execute_async(
        const A2ATask& task,
        std::string_view master_agent_id,
        std::shared_ptr<messaging::Context<A2ATask>> context
    ) -> void;

    // ========== Router Functions (New) ==========

    /**
     * @brief Smart execution (auto-classify + route + execute)
     *
     * Integrates Router functionality, users only need to call this one method
     *
     * **Execution Flow**:
     * 1. Task classification (TaskClassifier): identify task type
     * 2. Routing decision (TaskRouter): select Agent and collaboration mode
     * 3. Convert to A2A Task
     * 4. Execute based on routing decision, select collaboration mode
     *
     * @param instruction User instruction (natural language)
     * @param context Asynchronous context
     */
    auto smart_execute_async(
        std::string_view instruction,
        std::shared_ptr<messaging::Context<A2ATask>> context
    ) -> void;

    /**
     * @brief Smart execution (with Task object)
     *
     * @param task Task object
     * @param context Asynchronous context
     */
    auto smart_execute_async(
        const model::Task& task,
        std::shared_ptr<messaging::Context<A2ATask>> context
    ) -> void;

    /**
     * @brief Parse task (Router function)
     *
     * @param instruction User instruction
     * @return Task classification result
     */
    [[nodiscard]] auto parse_task(std::string_view instruction) const
        -> TaskClassification;

    /**
     * @brief Routing decision (Router function)
     *
     * @param classification Task classification
     * @return Routing decision
     */
    [[nodiscard]] auto route_task(const TaskClassification& classification) const
        -> RoutingDecision;

    // ========== Accessors ==========

    /**
     * @brief Get team ID
     */
    [[nodiscard]] auto get_team_id() const noexcept -> const std::string& {
        return team_id_;
    }

    /**
     * @brief Get collaboration mode
     */
    [[nodiscard]] auto get_mode() const noexcept -> CollaborationMode {
        return mode_;
    }

    /**
     * @brief Get message queue
     */
    [[nodiscard]] auto get_message_queue() const noexcept
        -> std::shared_ptr<messaging::IMessageQueue> {
        return message_queue_;
    }

    // ========== Layered Communication Architecture: Member Discovery & A2A Connections ==========

    /**
     * @brief Query team members from MessageBus
     *
     * Send TEAM_DISCOVER_MEMBERS message to AgentDiscoveryService,
     * get all member information of Team (including a2a_address)
     *
     * @return true indicates successful query
     *
     * @note Member information is cached in members_
     * @note Thread-safe
     */
    auto discover_members() -> bool;

    /**
     * @brief Subscribe to Team member change events
     *
     * Subscribe to TEAM_MEMBER_JOINED and TEAM_MEMBER_LEFT events
     * in the "team_updates" queue, automatically update local member list
     *
     * @note Should be called once at startup
     * @note Thread-safe
     */
    auto subscribe_team_updates() -> void;

    /**
     * @brief Member joined callback
     *
     * Called when TEAM_MEMBER_JOINED event is received
     *
     * @param info Information of the newly joined Agent
     *
     * @note Automatically establishes A2A connection
     * @note Thread-safe
     */
    auto on_member_joined(const messaging::AgentInfo& info) -> void;

    /**
     * @brief Member left callback
     *
     * Called when TEAM_MEMBER_LEFT event is received
     *
     * @param agent_id ID of the Agent that left
     *
     * @note Automatically cleans up A2A connection
     * @note Thread-safe
     */
    auto on_member_left(const std::string& agent_id) -> void;

    /**
     * @brief Establish intra-Team A2A PUB-SUB connections
     *
     * Establish A2A PUB-SUB connections for all team members:
     * - Create A2A Publisher (for broadcasting messages to Team)
     * - Create A2A Subscriber (for receiving Team messages)
     *
     * @return true indicates successful connection
     *
     * @note All Team members subscribe to the same topic: {team_id}.updates
     * @note Thread-safe
     */
    auto establish_a2a_connections() -> bool;

    /**
     * @brief Broadcast message to all Team members
     *
     * Broadcast message to all Team members via A2A PUB-SUB
     *
     * @param message Message to broadcast
     *
     * @note Uses A2A protocol (low latency < 3ms)
     * @note Thread-safe
     */
    auto broadcast_to_team(const A2AMessage& message) -> void;

    /**
     * @brief Get Team member information cache
     *
     * @return Agent information mapping (agent_id -> AgentInfo)
     *
     * @note Thread-safe (shared lock)
     */
    [[nodiscard]] auto get_members() const
        -> std::unordered_map<std::string, messaging::AgentInfo>;

    /**
     * @brief Get Team A2A topic
     *
     * @return A2A PUB-SUB topic (e.g., "team_a.updates")
     */
    [[nodiscard]] auto get_team_topic() const noexcept -> const std::string& {
        return team_topic_;
    }

    /**
     * @brief Set MessageBusClient reference
     *
     * @param message_bus_client MessageBusClient pointer
     *
     * @note Used for member discovery and event subscription
     */
    auto set_message_bus_client(messaging::MessageBusClient* message_bus_client) -> void {
        message_bus_client_ = message_bus_client;
    }

    /**
     * @brief Set TeamRegistry reference
     *
     * @param registry TeamRegistry pointer
     *
     * @note Used for querying member information
     */
    auto set_team_registry(TeamRegistry* registry) -> void {
        registry_ = registry;
    }

    // ========== Layered Communication Architecture: Team Resource Sharing ==========

    /**
     * @brief Create temporary Agent to execute task
     *
     * Create a lightweight Agent, automatically destroyed after task completion and returned to Pool.
     *
     * @param config Agent configuration
     * @return Agent smart pointer (automatic lifecycle management)
     *
     * @note Agent is lightweight, only holds configuration and state
     * @note Shares Team-level LLM client and rate limiter
     * @note Automatically destroyed after task completion, returned to AgentPool
     * @note Thread-safe
     */
    [[nodiscard]] auto create_temporary_agent(const model::AgentConfig& config)
        -> std::unique_ptr<Agent, std::function<void(Agent*)>>;

    /**
     * @brief Destroy Agent
     *
     * Return Agent to AgentPool for subsequent reuse.
     *
     * @param agent Agent pointer
     *
     * @note Usually no need to call manually, smart pointer handles automatically
     * @note Thread-safe
     */
    auto destroy_agent(Agent* agent) -> void;

    /**
     * @brief Check Team LLM rate limit
     *
     * @return true indicates request allowed, false indicates rate limited
     *
     * @note Team-level rate limiting (shared by all Agents)
     * @note Thread-safe
     */
    auto check_llm_rate_limit() const -> bool;

    /**
     * @brief Get Team's LLM client
     *
     * @return LLM client pointer (shared)
     *
     * @note All Agents share the same LLM client
     * @note Thread-safe (read-only access)
     */
    [[nodiscard]] auto get_llm_client() const
        -> std::shared_ptr<void>;  // TODO: 替换为实际的 LLMClient 类型

    /**
     * @brief Set LLM client
     *
     * @param client LLM client (shared)
     *
     * @note Team-level resource sharing
     */
    auto set_llm_client(std::shared_ptr<void> client) -> void;

    /**
     * @brief Get AgentPool
     *
     * @return AgentPool pointer
     *
     * @note Used for Agent reuse
     */
    [[nodiscard]] auto get_agent_pool() const
        -> std::shared_ptr<AgentPool>;

    /**
     * @brief Set AgentPool
     *
     * @param pool AgentPool (shared)
     *
     * @note Used for Agent reuse
     */
    auto set_agent_pool(std::shared_ptr<AgentPool> pool) -> void;

    /**
     * @brief Get rate limiter
     *
     * @return Rate limiter pointer
     *
     * @note Team-level LLM rate limiting
     */
    [[nodiscard]] auto get_rate_limiter() const
        -> std::shared_ptr<moltcat::utils::RateLimiter>;

    /**
     * @brief Set rate limiter
     *
     * @param limiter Rate limiter (shared)
     *
     * @note Team-level LLM rate limiting
     */
    auto set_rate_limiter(std::shared_ptr<moltcat::utils::RateLimiter> limiter) -> void;

private:
    // ========== Internal Helper Methods ==========

    /**
     * @brief Send task message to specified Agent
     */
    [[nodiscard]] auto send_task_to_agent(
        const A2ATask& task,
        std::string_view agent_id,
        TeamMessageType type = TeamMessageType::TASK_REQUEST
    ) -> bool;

    /**
     * @brief Receive result from specified Agent
     */
    [[nodiscard]] auto receive_result_from_agent(
        std::string_view agent_id,
        int timeout_ms = 5000
    ) -> std::optional<A2ATask>;

    /**
     * @brief Handle result returned from Agent (callback)
     *
     * @param agent_id Agent ID
     * @param result Execution result
     * @param context Asynchronous context
     */
    auto handle_agent_result(
        std::string_view agent_id,
        const A2ATask& result,
        std::shared_ptr<messaging::Context<A2ATask>> context
    ) -> void;

    /**
     * @brief Cancel task of specified Agent
     */
    auto cancel_agent_task(std::string_view agent_id, std::string_view task_id) -> void;

    /**
     * @brief Generate queue endpoint based on Agent ID
     *
     * Format: tcp://localhost:5500 + agent_id hash
     */
    [[nodiscard]] auto get_agent_endpoint(std::string_view agent_id) const -> std::string;

    // ========== Member Variables ==========

    std::string team_id_;                                       // Team ID
    CollaborationMode mode_;                                    // Collaboration mode
    std::vector<std::string> agent_ids_;                        // Agent ID list (does not hold pointers)
    std::shared_ptr<messaging::IMessageQueue> message_queue_;   // Message queue
    mutable std::shared_mutex mutex_;                           // Thread-safe

    // Statistics
    std::atomic<uint64_t> tasks_sent_{0};                       // Number of tasks sent
    std::atomic<uint64_t> results_received_{0};                 // Number of results received
    std::atomic<uint64_t> pipeline_stages_completed_{0};        // Number of pipeline stages completed

    // Layered Communication Architecture: Member Discovery & A2A Connections
    std::string team_topic_;                                    // A2A PUB-SUB topic (e.g., "team_a.updates")
    std::unordered_map<std::string, messaging::AgentInfo> members_;  // Member information cache (agent_id -> AgentInfo)

    messaging::MessageBusClient* message_bus_client_ = nullptr;  // MessageBusClient reference (does not own)
    TeamRegistry* registry_ = nullptr;                          // TeamRegistry reference (does not own)

    // A2A connections (TODO: add A2A Publisher/Subscriber in future implementation)
    // std::shared_ptr<A2APublisher> a2a_publisher_;
    // std::shared_ptr<A2ASubscriber> a2a_subscriber_;

    // Team Resource Sharing
    std::shared_ptr<void> llm_client_;                           // LLM client (shared)
    std::shared_ptr<AgentPool> agent_pool_;                      // Agent object pool
    std::shared_ptr<moltcat::utils::RateLimiter> llm_rate_limiter_;  // LLM rate limiter
};

} // namespace moltcat::agent
