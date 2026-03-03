#include "llm_client_factory.hpp"
#include "mock_llm_client.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <format>

namespace moltcat::network {

auto LlmClientFactory::create(
    const model::AgentConfig& config
) -> std::unique_ptr<ILlmClient> {
    const auto& provider = config.provider;

    MOLT_LOGGER.info("Creating LLM client: provider={}, model={}",
                     provider, config.model_name);

    auto& reg = registry();
    auto it = reg.find(provider);

    if (it == reg.end()) {
        MOLT_LOGGER.error("Unsupported provider: {}", provider);
        return nullptr;
    }

    try {
        return it->second(config);
    } catch (const std::exception& e) {
        MOLT_LOGGER.error("Failed to create LLM client: provider={}, error={}",
                         provider, e.what());
        return nullptr;
    }
}

auto LlmClientFactory::register_provider(
    std::string_view provider_name,
    CreatorFunc creator
) -> void {
    registry()[std::string(provider_name)] = std::move(creator);
    MOLT_LOGGER.info("Registered LLM provider: {}", provider_name);
}

auto LlmClientFactory::is_supported(std::string_view provider)
    -> bool {
    auto& reg = registry();
    return reg.find(std::string(provider)) != reg.end();
}

auto LlmClientFactory::get_supported_providers()
    -> std::vector<std::string> {
    std::vector<std::string> providers;
    auto& reg = registry();

    providers.reserve(reg.size());
    for (const auto& [name, _] : reg) {
        providers.push_back(name);
    }

    // Sort alphabetically
    std::sort(providers.begin(), providers.end());

    return providers;
}

auto LlmClientFactory::register_builtin_providers() -> void {
    // Mock provider (for testing)
    register_provider("mock", [](const model::AgentConfig& config)
        -> std::unique_ptr<ILlmClient> {
        // Mock client ignores API key and other configurations
        return std::make_unique<MockLlmClient>(
            500,  // Default 500ms delay
            false  // Default no error simulation
        );
    });

    // TODO: Add real API providers later
    // OpenAI
    // register_provider("openai", [](const auto& config) {
    //     return std::make_unique<OpenAIClient>(config);
    // });

    // Ollama
    // register_provider("ollama", [](const auto& config) {
    //     return std::make_unique<OllamaClient>(config);
    // });

    // Anthropic
    // register_provider("anthropic", [](const auto& config) {
    //     return std::make_unique<AnthropicClient>(config);
    // });

    MOLT_LOGGER.info("Registered built-in LLM providers: mock (openai/ollama/anthropic to be implemented)");
}

auto LlmClientFactory::registry() -> std::map<std::string, CreatorFunc>& {
    static std::map<std::string, CreatorFunc> registry_;

    // Register built-in providers on first call
    static std::once_flag init_flag;
    std::call_once(init_flag, [] {
        register_builtin_providers();
    });

    return registry_;
}

} // namespace moltcat::network
