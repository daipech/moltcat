#pragma once

#include "agent/agent.hpp"
#include "model/agent_config.hpp"
#include <memory>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <string>
#include <functional>

namespace moltcat::agent {

/**
 * AgentPool - Agent object pool
 *
 * Manages creation, reuse, and destruction of lightweight Agent instances.
 *
 * Core concepts:
 * - Lightweight Agent: only holds configuration and state, shares LLM client and other resources
 * - Object reuse: avoid overhead of frequent Agent creation/destruction
 * - Automatic management: Agents automatically return to pool after use
 *
 * Workflow:
 * 1. Agent creation: acquire from Pool or create new Agent
 * 2. Agent usage: execute tasks
 * 3. Agent destruction: automatically return to Pool (reuse)
 *
 * Performance advantages:
 * - Reduce memory allocation
 * - Reduce object initialization overhead
 * - Improve resource utilization
 */
class AgentPool {
public:
    /**
     * Agent pointer type (with custom deleter)
     */
    using AgentPtr = std::unique_ptr<Agent, std::function<void(Agent*)>>;

    /**
     * Agent factory function type
     *
     * Used to create Agent instances
     */
    using AgentFactory = std::function<std::unique_ptr<Agent>(const model::AgentConfig&)>;

    // ================================
    // Constructors
    // ================================

    /**
     * Constructor
     *
     * @param factory Agent factory function
     * @param max_pool_size Maximum pool size (0 means unlimited)
     * @param initial_size Initial pool size
     */
    explicit AgentPool(
        AgentFactory factory,
        size_t max_pool_size = 100,
        size_t initial_size = 0
    );

    ~AgentPool();

    // Disable copy and move
    AgentPool(const AgentPool&) = delete;
    AgentPool& operator=(const AgentPool&) = delete;
    AgentPool(AgentPool&&) = delete;
    AgentPool& operator=(AgentPool&&) = delete;

    // ================================
    // Agent Acquisition and Return
    // ================================

    /**
     * Acquire Agent (based on configuration)
     *
     * If there is an available Agent in the pool, reuse the existing instance;
     * otherwise create a new Agent instance.
     *
     * @param config Agent configuration
     * @return Agent smart pointer (automatically returns to pool after use)
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto acquire(const model::AgentConfig& config) -> AgentPtr;

    /**
     * Return Agent
     *
     * Return Agent to the pool for subsequent reuse.
     *
     * @param agent Agent raw pointer
     *
     * @note Usually no need to call manually, smart pointer handles automatically
     * @note Thread-safe
     */
    auto release(Agent* agent) -> void;

    // ================================
    // Pool Management
    // ================================

    /**
     * Clear pool
     *
     * Release all Agent instances in the pool.
     *
     * @note Thread-safe
     */
    auto clear() -> void;

    /**
     * Warm up pool
     *
     * Pre-create specified number of Agent instances.
     *
     * @param config Agent configuration
     * @param count Warm-up count
     *
     * @note Thread-safe
     */
    auto warm_up(const model::AgentConfig& config, size_t count) -> void;

    // ================================
    // Statistics
    // ================================

    /**
     * Pool statistics
     */
    struct PoolStats {
        size_t total_created = 0;      // Total created
        size_t total_reused = 0;       // Total reused
        size_t total_released = 0;     // Total returned
        size_t current_size = 0;       // Current pool size (available count)
        size_t active_count = 0;       // Active Agent count
    };

    /**
     * Get statistics
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto get_stats() const -> PoolStats;

    /**
     * Get maximum pool size
     */
    [[nodiscard]] auto get_max_pool_size() const noexcept -> size_t {
        return max_pool_size_;
    }

    /**
     * Set maximum pool size
     *
     * @param size Maximum size (0 means unlimited)
     *
     * @note If current pool size exceeds new limit, excess Agents will be released
     * @note Thread-safe
     */
    auto set_max_pool_size(size_t size) -> void;

private:
    /**
     * 创建新的 Agent 实例
     */
    [[nodiscard]] auto create_agent(const model::AgentConfig& config) const
        -> std::unique_ptr<Agent>;

    /**
     * 将 Agent 归还到池（内部实现）
     */
    auto release_to_pool(std::unique_ptr<Agent> agent) -> void;

    // ================================
    // 字段
    // ================================

    AgentFactory factory_;                    // Agent 工厂函数
    size_t max_pool_size_;                    // 池最大大小

    // Agent 池（按 agent_type 分组）
    std::unordered_map<std::string, std::queue<std::unique_ptr<Agent>>> pools_;
    mutable std::mutex pools_mutex_;          // 保护 pools_

    // 统计信息
    mutable std::mutex stats_mutex_;
    PoolStats stats_;
};

/**
 * 辅助函数：创建默认的 Agent 工厂
 *
 * @return Agent 工厂函数
 *
 * @note 使用 AgentTemplate 创建 Agent
 */
auto create_default_agent_factory() -> AgentPool::AgentFactory;

} // namespace moltcat::agent
