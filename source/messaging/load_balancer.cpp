#include "load_balancer.hpp"
#include <spdlog/spdlog.h>

namespace moltcat::messaging {

// ================================
// Constructor
// ================================

LoadBalancer::LoadBalancer(LoadBalanceStrategy strategy)
    : strategy_(strategy)
    , round_robin_index_(0)
{
    spdlog::get("moltcat")->info("Load balancer initialized, strategy: {}",
        strategy == LoadBalanceStrategy::ROUND_ROBIN ? "round-robin" : "random");
}

// ================================
// Select Queue
// ================================

auto LoadBalancer::select_queue(const std::vector<std::string>& queues)
    -> std::string {
    if (queues.empty()) {
        return "";
    }

    if (queues.size() == 1) {
        return queues[0];
    }

    switch (strategy_) {
        case LoadBalanceStrategy::ROUND_ROBIN:
            return select_round_robin(queues);

        case LoadBalanceStrategy::RANDOM:
            return select_random(queues);
    }

    // Should not reach here
    return queues[0];
}

// ================================
// Set Strategy
// ================================

auto LoadBalancer::set_strategy(LoadBalanceStrategy strategy) -> void {
    strategy_ = strategy;
    spdlog::get("moltcat")->info("Load balance strategy changed to: {}",
        strategy == LoadBalanceStrategy::ROUND_ROBIN ? "round-robin" : "random");
}

// ================================
// Round-robin Selection
// ================================

auto LoadBalancer::select_round_robin(const std::vector<std::string>& queues)
    -> std::string {
    size_t index = round_robin_index_.fetch_add(1) % queues.size();
    return queues[index];
}

// ================================
// Random Selection
// ================================

auto LoadBalancer::select_random(const std::vector<std::string>& queues)
    -> std::string {
    thread_local RandomGenerator rng;

    std::uniform_int_distribution<size_t> dist(0, queues.size() - 1);
    size_t index = dist(rng.gen);

    return queues[index];
}

} // namespace moltcat::messaging
