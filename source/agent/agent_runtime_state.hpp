#pragma once

#include "model/types.hpp"
#include <atomic>
#include <cstdint>
#include <chrono>

namespace moltcat::agent {

/**
 * @brief Agent runtime dynamic state
 *
 * Thread-safe dynamic state management, all fields protected by atomic operations
 * Responsible for tracking real-time status and statistics of Agent during runtime
 */
class AgentRuntimeState {
public:
    AgentRuntimeState() = default;

    // ========== Status Query ==========

    /**
     * @brief Get Agent current state
     */
    [[nodiscard]] auto get_state() const noexcept -> AgentState {
        return state_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get current active task count
     */
    [[nodiscard]] auto get_active_tasks() const noexcept -> uint32_t {
        return active_tasks_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get cumulative executed task count
     */
    [[nodiscard]] auto get_total_executed() const noexcept -> uint64_t {
        return total_executed_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get cumulative execution time (milliseconds)
     */
    [[nodiscard]] auto get_total_execution_time_ms() const noexcept -> uint64_t {
        return total_execution_time_ms_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get average response time (milliseconds)
     */
    [[nodiscard]] auto get_average_response_time_ms() const noexcept -> double {
        auto total = total_executed_.load(std::memory_order_relaxed);
        if (total == 0) return 0.0;
        auto total_time = total_execution_time_ms_.load(std::memory_order_relaxed);
        return static_cast<double>(total_time) / static_cast<double>(total);
    }

    // ========== Status Update ==========

    /**
     * @brief Set Agent state
     */
    auto set_state(AgentState new_state) noexcept -> void {
        state_.store(new_state, std::memory_order_release);
    }

    /**
     * @brief Set to running state
     */
    auto set_running() noexcept -> void {
        set_state(AgentState::RUNNING);
    }

    /**
     * @brief Set to idle state
     */
    auto set_idle() noexcept -> void {
        set_state(AgentState::IDLE);
    }

    /**
     * @brief Set to error state
     */
    auto set_error() noexcept -> void {
        set_state(AgentState::ERROR);
    }

    /**
     * @brief Set to offline state
     */
    auto set_offline() noexcept -> void {
        set_state(AgentState::OFFLINE);
    }

    // ========== Task Count Management ==========

    /**
     * @brief Increment active task count
     */
    auto increment_active_tasks() noexcept -> uint32_t {
        return active_tasks_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    /**
     * @brief Decrement active task count
     */
    auto decrement_active_tasks() noexcept -> uint32_t {
        auto old = active_tasks_.fetch_sub(1, std::memory_order_relaxed);
        return old > 0 ? old - 1 : 0;
    }

    // ========== Execution Statistics ==========

    /**
     * @brief Record task execution
     */
    auto record_execution(uint64_t duration_ms) noexcept -> void {
        total_executed_.fetch_add(1, std::memory_order_relaxed);
        total_execution_time_ms_.fetch_add(duration_ms, std::memory_order_relaxed);
    }

    /**
     * @brief Reset statistics
     */
    auto reset_statistics() noexcept -> void {
        total_executed_.store(0, std::memory_order_relaxed);
        total_execution_time_ms_.store(0, std::memory_order_relaxed);
    }

    // ========== Rate Limit Management ==========

    /**
     * @brief Check if rate limit is exceeded
     */
    [[nodiscard]] auto check_rate_limit(uint32_t max_requests_per_minute,
                                         uint64_t time_window_ms = 60000) const noexcept -> bool {
        auto current_requests = requests_in_current_minute_.load(std::memory_order_relaxed);
        auto last_request = last_request_time_.load(std::memory_order_relaxed);

        if (current_requests >= max_requests_per_minute) {
            // Check if time window has passed
            auto now = get_current_time_ms();
            if (now - last_request < time_window_ms) {
                return false;  // Rate limit exceeded
            }
        }

        return true;
    }

    /**
     * @brief Increment request count
     */
    auto increment_request_count() noexcept -> void {
        requests_in_current_minute_.fetch_add(1, std::memory_order_relaxed);
        last_request_time_.store(get_current_time_ms(), std::memory_order_relaxed);
    }

    /**
     * @brief Reset request count
     */
    auto reset_request_count() noexcept -> void {
        requests_in_current_minute_.store(0, std::memory_order_relaxed);
    }

    // ========== Token Usage Statistics ==========

    /**
     * @brief Record Token usage
     */
    auto record_token_usage(uint64_t tokens) noexcept -> void {
        total_tokens_used_.fetch_add(tokens, std::memory_order_relaxed);
    }

    /**
     * @brief Get cumulative used Token count
     */
    [[nodiscard]] auto get_total_tokens_used() const noexcept -> uint64_t {
        return total_tokens_used_.load(std::memory_order_relaxed);
    }

    // ========== Resource Usage Monitoring ==========

    /**
     * @brief Update CPU usage rate
     */
    auto set_cpu_usage(double percent) noexcept -> void {
        cpu_usage_percent_.store(percent, std::memory_order_relaxed);
    }

    /**
     * @brief Get CPU usage rate
     */
    [[nodiscard]] auto get_cpu_usage() const noexcept -> double {
        return cpu_usage_percent_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Update memory usage
     */
    auto set_memory_usage(uint64_t bytes) noexcept -> void {
        memory_usage_bytes_.store(bytes, std::memory_order_relaxed);
    }

    /**
     * @brief Get memory usage
     */
    [[nodiscard]] auto get_memory_usage() const noexcept -> uint64_t {
        return memory_usage_bytes_.load(std::memory_order_relaxed);
    }

private:
    /**
     * @brief Get current timestamp (milliseconds)
     */
    [[nodiscard]] static auto get_current_time_ms() noexcept -> uint64_t {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    // ========== Agent Status ==========
    std::atomic<AgentState> state_{AgentState::OFFLINE};

    // ========== Task Statistics ==========
    std::atomic<uint32_t> active_tasks_{0};
    std::atomic<uint64_t> total_executed_{0};
    std::atomic<uint64_t> total_execution_time_ms_{0};

    // ========== Rate Limit Status ==========
    std::atomic<uint64_t> last_request_time_{0};
    std::atomic<uint32_t> requests_in_current_minute_{0};

    // ========== Token Statistics ==========
    std::atomic<uint64_t> total_tokens_used_{0};

    // ========== Resource Usage ==========
    std::atomic<double> cpu_usage_percent_{0.0};
    std::atomic<uint64_t> memory_usage_bytes_{0};
};

} // namespace moltcat::agent
