#pragma once

#include "llm_request.hpp"
#include "llm_response.hpp"
#include <functional>
#include <memory>
#include <string>

namespace moltcat::network {

/**
 * @brief LLM client abstract interface (fully asynchronous)
 *
 * Defines unified LLM API call interface, supports multi-provider implementations
 * Only provides asynchronous call methods, leveraging existing HybridExecutor execution framework
 */
class ILlmClient {
public:
    virtual ~ILlmClient() = default;

    /**
     * @brief Asynchronously call LLM API
     *
     * @param request LLM request
     * @param callback Completion callback (called in executor thread)
     *
     * @note Callback function will be called in HybridExecutor's worker thread
     * @note Implementers need to ensure thread safety
     */
    virtual auto chat_async(
        const LlmRequest& request,
        std::function<void(LlmResponse)> callback
    ) -> void = 0;

    /**
     * @brief Check if client is available
     *
     * @return true Client available
     * @return false Client unavailable (e.g., API key not configured)
     */
    [[nodiscard]] virtual auto is_available() const -> bool = 0;

    /**
     * @brief Get provider name
     *
     * @return Provider name (e.g., "openai", "anthropic", "ollama", "mock")
     */
    [[nodiscard]] virtual auto get_provider() const
        -> std::string_view = 0;
};

} // namespace moltcat::network
