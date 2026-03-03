#include "agent/agent_pool.hpp"
#include "agent/agent_template.hpp"
#include <spdlog/spdlog.h>

namespace moltcat::agent {

// ================================
// Constructor and Destructor
// ================================

AgentPool::AgentPool(
    AgentFactory factory,
    size_t max_pool_size,
    size_t initial_size
)
    : factory_(std::move(factory))
    , max_pool_size_(max_pool_size)
{
    spdlog::info("AgentPool initialized: max_size={}, initial_size={}",
                 max_pool_size, initial_size);

    // If initial size specified, warm up pool
    if (initial_size > 0) {
        // Warm up using default configuration
        model::AgentConfig default_config;
        warm_up(default_config, initial_size);
    }
}

AgentPool::~AgentPool() {
    spdlog::info("AgentPool destroyed: created={}, reused={}, released={}",
                 stats_.total_created, stats_.total_reused, stats_.total_released);

    // Clear pool
    clear();
}

// ================================
// Agent Acquisition and Return
// ================================

auto AgentPool::acquire(const model::AgentConfig& config) -> AgentPtr {
    std::unique_lock lock(pools_mutex_);

    std::string agent_type = config.agent_type.empty() ? "default" : config.agent_type;
    auto& pool = pools_[agent_type];

    std::unique_ptr<Agent> agent;

    // 1. Try to acquire from pool
    if (!pool.empty()) {
        agent = std::move(pool.front());
        pool.pop();

        stats_.total_reused++;
        spdlog::debug("AgentPool reused Agent: type={}, pool_size={}",
                      agent_type, pool.size());

    } else {
        // 2. No available Agent in pool, create new one
        lock.unlock();
        agent = create_agent(config);
        lock.lock();

        stats_.total_created++;
        spdlog::debug("AgentPool created new Agent: type={}", agent_type);
    }

    // 3. Update statistics
    stats_.active_count++;
    stats_.current_size = pool.size();

    // 4. Return smart pointer with custom deleter
    //    When smart pointer is destroyed, automatically return Agent to pool
    AgentPtr agent_ptr(
        agent.release(),
        [this](Agent* a) {
            this->release(a);
        }
    );

    return agent_ptr;
}

auto AgentPool::release(Agent* agent) -> void {
    if (!agent) {
        return;
    }

    // Wrap Agent in unique_ptr and return to pool
    std::unique_ptr<Agent> agent_ptr(agent);
    release_to_pool(std::move(agent_ptr));
}

auto AgentPool::release_to_pool(std::unique_ptr<Agent> agent) -> void {
    std::unique_lock lock(pools_mutex_);

    stats_.total_released++;
    stats_.active_count--;

    // Get Agent type
    std::string agent_type = agent->get_config().agent_type;
    if (agent_type.empty()) {
        agent_type = "default";
    }

    auto& pool = pools_[agent_type];

    // Check if pool is full
    if (pool.size() >= max_pool_size_) {
        // Pool is full, discard Agent
        spdlog::debug("AgentPool pool full, discarding Agent: type={}, size={}",
                      agent_type, pool.size());
        return;
    }

    // Return Agent to pool
    pool.push(std::move(agent));
    stats_.current_size = pool.size();

    spdlog::debug("AgentPool returned Agent: type={}, pool_size={}",
                  agent_type, pool.size());
}

// ================================
// Pool Management
// ================================

auto AgentPool::clear() -> void {
    std::unique_lock lock(pools_mutex_);

    size_t total_count = 0;
    for (auto& [type, pool] : pools_) {
        total_count += pool.size();
        while (!pool.empty()) {
            pool.pop();
        }
    }

    pools_.clear();
    stats_.current_size = 0;

    spdlog::info("AgentPool cleared: released {} Agents", total_count);
}

auto AgentPool::warm_up(const model::AgentConfig& config, size_t count) -> void {
    spdlog::info("AgentPool warm up: count={}, type={}",
                 count, config.agent_type);

    for (size_t i = 0; i < count; ++i) {
        auto agent = create_agent(config);
        release_to_pool(std::move(agent));
    }
}

// ================================
// Statistics
// ================================

auto AgentPool::get_stats() const -> PoolStats {
    std::unique_lock lock(stats_mutex_);
    return stats_;
}

auto AgentPool::set_max_pool_size(size_t size) -> void {
    std::unique_lock lock(pools_mutex_);

    spdlog::info("AgentPool set max pool size: {} -> {}", max_pool_size_, size);

    max_pool_size_ = size;

    // If current pool size exceeds new limit, release excess Agents
    for (auto& [type, pool] : pools_) {
        while (pool.size() > max_pool_size_) {
            pool.pop();
        }
    }

    // Update statistics
    size_t total_size = 0;
    for (const auto& [_, pool] : pools_) {
        total_size += pool.size();
    }
    stats_.current_size = total_size;
}

// ================================
// Internal Methods
// ================================

auto AgentPool::create_agent(const model::AgentConfig& config) const
    -> std::unique_ptr<Agent>
{
    if (!factory_) {
        throw std::runtime_error("Agent factory not set");
    }

    return factory_(config);
}

// ================================
// Helper Functions
// ================================

auto create_default_agent_factory() -> AgentPool::AgentFactory {
    return [](const model::AgentConfig& config) -> std::unique_ptr<Agent> {
        // TODO: Use AgentTemplate to create Agent
        //
        // Current implementation: returns null pointer (need to integrate actual Agent creation logic)
        //
        // Actual implementation should be:
        // auto template = AgentTemplate::create_default();
        // return template.create_instance(config.agent_id, config);

        spdlog::warn("AgentPool: using default factory (not implemented)");
        return nullptr;
    };
}

} // namespace moltcat::agent
