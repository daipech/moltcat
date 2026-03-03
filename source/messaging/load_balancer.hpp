#pragma once

#include <string>
#include <vector>
#include <random>
#include <atomic>

namespace moltcat::messaging {

/**
 * Load balancing strategy
 */
enum class LoadBalanceStrategy {
    ROUND_ROBIN,  // Round-robin
    RANDOM        // Random
};

/**
 * Load balancer
 *
 * Responsibilities:
 * 1. Select one queue from multiple queues
 * 2. Implement different load balancing strategies
 *
 * Design notes:
 * - Stateless design, each selection is independent
 * - ROUND_ROBIN uses atomic counter, thread-safe
 * - RANDOM uses random number generator
 */
class LoadBalancer {
public:
    explicit LoadBalancer(LoadBalanceStrategy strategy);
    ~LoadBalancer() = default;

    // Disable copy and move
    LoadBalancer(const LoadBalancer&) = delete;
    LoadBalancer& operator=(const LoadBalancer&) = delete;
    LoadBalancer(LoadBalancer&&) = delete;
    LoadBalancer& operator=(LoadBalancer&&) = delete;

    /**
     * Select a queue
     *
     * @param queues Candidate queue list
     * @return Selected queue ID, returns empty string if list is empty
     */
    auto select_queue(const std::vector<std::string>& queues)
        -> std::string;

    /**
     * Set load balancing strategy
     *
     * @param strategy New strategy
     */
    auto set_strategy(LoadBalanceStrategy strategy) -> void;

    /**
     * Get current strategy
     */
    auto get_strategy() const -> LoadBalanceStrategy {
        return strategy_;
    }

private:
    LoadBalanceStrategy strategy_;
    std::atomic<size_t> round_robin_index_{0};

    // Random number generator (thread-local storage)
    struct RandomGenerator {
        std::random_device rd;
        std::mt19937 gen;

        RandomGenerator() : gen(rd()) {}
    };

    auto select_round_robin(const std::vector<std::string>& queues)
        -> std::string;

    auto select_random(const std::vector<std::string>& queues)
        -> std::string;
};

} // namespace moltcat::messaging
