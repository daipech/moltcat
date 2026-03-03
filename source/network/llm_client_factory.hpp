#pragma once

#include "llm_client.hpp"
#include "model/agent_config.hpp"
#include <memory>
#include <string>
#include <map>
#include <functional>

namespace moltcat::network {

/**
 * @brief LLM client factory
 *
 * Create corresponding client instances based on provider configuration
 * Supports custom provider registration
 */
class LlmClientFactory {
public:
    /**
     * @brief Client creation function signature
     *
     * @param config Agent configuration
     * @return Client instance
     */
    using CreatorFunc = std::function<std::unique_ptr<ILlmClient>(
        const model::AgentConfig&
    )>;

    /**
     * @brief Create client from configuration
     *
     * @param config Agent configuration
     * @return Client instance, returns nullptr on failure
     */
    [[nodiscard]] static auto create(
        const model::AgentConfig& config
    ) -> std::unique_ptr<ILlmClient>;

    /**
     * @brief Register custom provider
     *
     * @param provider_name Provider name
     * @param creator Creation function
     */
    static auto register_provider(
        std::string_view provider_name,
        CreatorFunc creator
    ) -> void;

    /**
     * @brief Check if provider is supported
     *
     * @param provider Provider name
     * @return true Supported
     * @return false Not supported
     */
    [[nodiscard]] static auto is_supported(
        std::string_view provider
    ) -> bool;

    /**
     * @brief Get all supported providers
     *
     * @return List of provider names
     */
    [[nodiscard]] static auto get_supported_providers()
        -> std::vector<std::string>;

private:
    /**
     * @brief Register built-in providers
     *
     * Register built-in providers such as mock, openai, ollama, anthropic, etc.
     */
    static auto register_builtin_providers() -> void;

    /**
     * @brief Get registry (singleton)
     *
     * @return Provider registry reference
     */
    static auto registry() -> std::map<std::string, CreatorFunc>&;
};

} // namespace moltcat::network
