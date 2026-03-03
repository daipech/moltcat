#pragma once

#include <glaze/glaze.hpp>
#include <string>
#include <chrono>
#include <cstdint>

namespace moltcat::model {

/**
 * @brief Provider configuration
 *
 * Defines LLM Provider configuration, including API keys, endpoints, connection pool, etc.
 * Multiple Agents can share the same Provider
 */
struct ProviderConfig {
    // ========== Provider identification ==========
    std::string provider_id;                    // Provider unique ID
    std::string provider_name;                  // Provider name ("openai", "ollama", "anthropic", "mock")

    // ========== API configuration ==========
    std::string api_key;                        // API key
    std::string api_endpoint;                   // API endpoint URL
    std::string api_version;                    // API version (optional)

    // ========== Connection pool configuration ==========
    uint32_t max_connections{5};                // Maximum number of connections
    uint64_t connection_timeout_ms{30000};      // Connection timeout
    uint64_t request_timeout_ms{60000};         // Request timeout

    // ========== Default model configuration ==========
    std::string default_model;                  // Default model name
    float default_temperature{0.7f};            // Default temperature
    uint32_t default_max_tokens{4096};          // Default max tokens

    // ========== Rate limiting configuration ==========
    uint32_t requests_per_minute{60};            // Requests per minute limit
    uint32_t max_retries{3};                    // Maximum retry count (not currently used, errors returned directly)

    // ========== Advanced configuration ==========
    bool enable_streaming{false};              // Enable streaming output
    bool enable_function_calling{false};       // Enable function calling
};

// Glaze JSON serialization support
template <>
struct glz::meta<ProviderConfig> {
    using T = ProviderConfig;
    static constexpr auto value = glz::object<
        "provider_id", &T::provider_id,
        "provider_name", &T::provider_name,
        "api_key", &T::api_key,
        "api_endpoint", &T::api_endpoint,
        "api_version", &T::api_version,
        "max_connections", &T::max_connections,
        "connection_timeout_ms", &T::connection_timeout_ms,
        "request_timeout_ms", &T::request_timeout_ms,
        "default_model", &T::default_model,
        "default_temperature", &T::default_temperature,
        "default_max_tokens", &T::default_max_tokens,
        "requests_per_minute", &T::requests_per_minute,
        "max_retries", &T::max_retries,
        "enable_streaming", &T::enable_streaming,
        "enable_function_calling", &T::enable_function_calling
    >;
};

} // namespace moltcat::model
