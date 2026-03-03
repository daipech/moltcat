#pragma once

#include "agent_template.hpp"
#include "agent_runtime_state.hpp"
#include "memory/agent_memory_view.hpp"
#include "provider/provider.hpp"
#include "messaging/context.hpp"
#include "protocol/a2a_types.hpp"
#include "protocol/a2a_adapter.hpp"
#include "protocol/a2a_memory_extensions.hpp"
#include "model/agent_config.hpp"
#include "model/task.hpp"
#include "model/result.hpp"
#include "model/context.hpp"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

namespace moltcat::agent {

// Forward declaration
class AgentTemplate;

// A2A type aliases
using A2AMessage = moltcat::protocol::a2a::Message;
using A2ATask = moltcat::protocol::a2a::Task;
using A2ARole = moltcat::protocol::a2a::Role;
using A2ATaskState = moltcat::protocol::a2a::TaskState;
using A2ATaskStatus = moltcat::protocol::a2a::TaskStatus;
using A2AArtifact = moltcat::protocol::a2a::Artifact;
using A2AAdapter = moltcat::protocol::adapter::A2AAdapter;

// Memory extensions namespace
namespace memory_extensions = moltcat::protocol::extensions;

/**
 * @brief Agent instance (fully asynchronous version)
 *
 * Runtime entity created from AgentTemplate with independent dynamic state and memory view
 *
 * Core responsibilities:
 * - Asynchronous task execution (non-blocking LLM calls)
 * - Memory management (automatic episodic memory recording)
 * - Lifecycle management (start/stop/restart)
 * - Rate limiting and concurrency control
 *
 * Architecture changes (Phase 3.1):
 * - ✅ Fully asynchronous: removed synchronous execute() method
 * - ✅ Callback chain processing: LLM response → memory recording → completion
 * - ✅ Context parameterization: all methods accept Context
 * - ✅ Asynchronous cleanup operations: memory recording etc. moved to callbacks
 */
class Agent {
public:
    /**
     * @brief Constructor
     *
     * @param instance_id Instance ID
     * @param templ Template (reference, shared configuration)
     */
    Agent(
        std::string instance_id,
        const AgentTemplate& templ
    );

    /**
     * @brief Constructor (with configuration overrides)
     *
     * @param instance_id Instance ID
     * @param templ Template (reference)
     * @param config_overrides Configuration overrides
     */
    Agent(
        std::string instance_id,
        const AgentTemplate& templ,
        const model::AgentConfig& config_overrides
    );

    // ========== Disable copy and move ==========
    Agent(const Agent&) = delete;
    Agent& operator=(const Agent&) = delete;
    Agent(Agent&&) = delete;
    Agent& operator=(Agent&&) = delete;

    /**
     * @brief Destructor
     */
    ~Agent();

    // ========== Accessors ==========

    /**
     * @brief Get instance ID
     */
    [[nodiscard]] auto get_instance_id() const noexcept -> const std::string& {
        return instance_id_;
    }

    /**
     * @brief Get template reference
     */
    [[nodiscard]] auto get_template() const noexcept -> const AgentTemplate& {
        return template_;
    }

    /**
     * @brief Get configuration (shared)
     */
    [[nodiscard]] auto get_config() const noexcept -> const model::AgentConfig& {
        return config_;
    }

    /**
     * @brief Get metadata (shared)
     */
    [[nodiscard]] auto get_metadata() const noexcept -> const model::AgentMetadata& {
        return metadata_;
    }

    /**
     * @brief Get runtime state
     */
    [[nodiscard]] auto get_runtime_state() const noexcept -> const AgentRuntimeState& {
        return runtime_state_;
    }

    /**
     * @brief Get Agent state
     */
    [[nodiscard]] auto get_state() const noexcept -> model::AgentState {
        return runtime_state_.get_state();
    }

    /**
     * @brief Get memory view
     */
    [[nodiscard]] auto get_memory_view() noexcept -> memory::AgentMemoryView& {
        return *memory_view_;
    }

    // ========== Lifecycle management ==========

    /**
     * @brief Start Agent
     */
    auto start() -> void;

    /**
     * @brief Stop Agent
     */
    auto stop() -> void;

    /**
     * @brief Restart Agent
     */
    auto restart() -> void;

    /**
     * @brief Check if Agent is available
     */
    [[nodiscard]] auto is_available() const noexcept -> bool;

    // ========== Asynchronous task execution (core) ==========

    /**
     * @brief Execute single task asynchronously (returns immediately, non-blocking)
     *
     * Execution flow:
     * 1. Check rate limiting
     * 2. Retrieve relevant memories (synchronous, fast)
     * 3. Call LLM API (asynchronous, non-blocking)
     * 4. Set result to Context after LLM returns
     *
     * @param task Task
     * @param context Asynchronous context (templated, using ModelContext = Context<model::Result>)
     */
    auto execute_async(
        const model::Task& task,
        std::shared_ptr<messaging::Context<model::Result>> context
    ) -> void;

    /**
     * @brief Execute task asynchronously (with model::Context)
     *
     * @param task Task
     * @param model_context MoltCat context
     * @param context Asynchronous context (templated)
     */
    auto execute_async(
        const model::Task& task,
        const model::Context& model_context,
        std::shared_ptr<messaging::Context<model::Result>> context
    ) -> void;

    /**
     * @brief Batch asynchronous execution
     *
     * @param tasks Task list
     * @param context Batch context (templated)
     */
    auto execute_batch_async(
        const std::vector<model::Task>& tasks,
        std::shared_ptr<messaging::Context<model::Result>> context
    ) -> void;

    // ========== Capability queries ==========

    /**
     * @brief Check if has a specific capability
     */
    [[nodiscard]] auto has_capability(std::string_view capability) const -> bool;

    /**
     * @brief Get all capabilities list
     */
    [[nodiscard]] auto get_capabilities() const -> std::vector<std::string>;

    // ========== Memory operations ==========

    /**
     * @brief Store private memory
     *
     * @param model_context Context
     * @param content Content
     * @param importance Importance [0.0, 1.0]
     * @return Memory ID
     */
    auto store_private_memory(
        const model::Context& model_context,
        std::string_view content,
        float importance = 0.5f
    ) -> std::string;

    /**
     * @brief Retrieve relevant memories
     *
     * @param query Query
     * @param top_k Return top K results
     * @return Memory content list
     */
    [[nodiscard]] auto retrieve_memories(
        std::string_view query,
        size_t top_k = 10
    ) const -> std::vector<std::string>;

    /**
     * @brief Get task-related memories
     *
     * @param task_id Task ID
     * @return Memory list
     */
    [[nodiscard]] auto get_task_memories(std::string_view task_id) const
        -> std::vector<memory::MemoryEntry>;

    // ========== A2A asynchronous task execution ==========

    /**
     * @brief Execute A2A message asynchronously (deprecated, use execute_async instead)
     *
     * A2A messages are now processed through execute_async
     * This method is kept for backward compatibility
     *
     * @param message A2A message
     * @param context Asynchronous context
     */
    auto execute_a2a_async(
        const A2AMessage& message,
        std::shared_ptr<messaging::Context<model::Result>> context
    ) -> void;

private:
    // ========== Instance identifiers ==========
    std::string instance_id_;                     // Instance ID

    // ========== Template reference (configuration sharing) ==========
    const AgentTemplate& template_;              // Template reference
    const model::AgentConfig& config_;            // Configuration reference (shared)
    const model::AgentMetadata& metadata_;        // Metadata reference (shared)

    // ========== Dynamic state (independent) ==========
    AgentRuntimeState runtime_state_;             // Runtime state
    std::unique_ptr<memory::AgentMemoryView> memory_view_; // Memory view

    // ========== Provider reference ==========
    provider::Provider* provider_{nullptr};      // Provider pointer (not responsible for lifecycle)

    // ========== Internal state ==========
    bool is_running_{false};                     // Is running

    // ========== Asynchronous callback methods ==========

    /**
     * @brief Call LLM asynchronously (core)
     *
     * @param task Task
     * @param relevant_memories Relevant memories
     * @param context Asynchronous context (templated)
     */
    auto call_llm_async(
        const model::Task& task,
        const std::vector<memory::MemoryEntry>& relevant_memories,
        std::shared_ptr<messaging::Context<model::Result>> context
    ) -> void;

    // ========== Helper methods ==========

    /**
     * @brief Retrieve relevant memories
     */
    [[nodiscard]] auto retrieve_relevant_memories(
        const model::Task& task
    ) const -> std::vector<memory::MemoryEntry>;

    /**
     * @brief Update request count (rate limiting)
     */
    auto update_request_count() -> void;
};

} // namespace moltcat::agent
