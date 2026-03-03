#pragma once

#include "llm_client.hpp"
#include <string>
#include <cstdint>

namespace moltcat::network {

/**
 * @brief Mock LLM client (for testing)
 *
 * Simulates asynchronous LLM calls without real API
 * Supports configurable delay and response content
 */
class MockLlmClient : public ILlmClient {
public:
    /**
     * @brief Constructor
     *
     * @param simulated_latency_ms Simulated API call delay (milliseconds)
     * @param simulate_error Whether to simulate error
     */
    explicit MockLlmClient(
        uint64_t simulated_latency_ms = 500,
        bool simulate_error = false
    );

    // ========== ILlmClient interface implementation ==========

    /**
     * @brief Asynchronous call (simulated in new thread)
     */
    auto chat_async(
        const LlmRequest& request,
        std::function<void(LlmResponse)> callback
    ) -> void override;

    [[nodiscard]] auto is_available() const -> bool override {
        return true;  // Mock always available
    }

    [[nodiscard]] auto get_provider() const
        -> std::string_view override {
        return "mock";
    }

    // ========== Test control methods ==========

    /**
     * @brief Set mock response content
     *
     * @param response Response content
     */
    auto set_mock_response(std::string response) -> void;

    /**
     * @brief Set whether to simulate error
     *
     * @param simulate true to simulate error, false for normal response
     */
    auto set_simulate_error(bool simulate) -> void;

    /**
     * @brief Set simulated delay
     *
     * @param latency_ms Delay time (milliseconds)
     */
    auto set_latency(uint64_t latency_ms) -> void;

private:
    uint64_t latency_ms_;
    bool simulate_error_;
    std::string mock_response_;

    /**
     * @brief Simulate asynchronous call in new thread
     */
    auto simulate_async_call(
        const LlmRequest& request,
        std::function<void(LlmResponse)> callback
    ) -> void;
};

} // namespace moltcat::network
