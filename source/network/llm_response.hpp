#pragma once

#include "model/result.hpp"
#include <glaze/glaze.hpp>
#include <string>
#include <cstdint>

namespace moltcat::network {

/**
 * @brief Token usage statistics
 */
struct TokenUsage {
    uint32_t prompt_tokens{0};
    uint32_t completion_tokens{0};
    uint32_t total_tokens{0};
};

/**
 * @brief LLM API response structure
 *
 * Unified response format, abstracting differences between different providers
 */
struct LlmResponse {
    // ========== Response status ==========
    bool success{false};
    std::string error_message;

    // ========== Content data ==========
    std::string content;                     // Generated text content
    std::string model;                       // Actually used model
    std::string finish_reason;               // "stop", "length", "content_filter"

    // ========== Usage statistics ==========
    TokenUsage usage;
    uint64_t latency_ms{0};                  // Request latency

    // ========== Metadata ==========
    std::string request_id;                  // Corresponding request ID
    std::string response_id;                 // Response ID

    /**
     * @brief Create success response
     *
     * @param content Response content
     * @return LlmResponse Success response object
     */
    [[nodiscard]] static auto ok(std::string content) -> LlmResponse;

    /**
     * @brief Create error response
     *
     * @param message Error message
     * @return LlmResponse Error response object
     */
    [[nodiscard]] static auto error(std::string message) -> LlmResponse;

    /**
     * @brief Convert to model::Result
     *
     * @return model::Result Result object used by Agent
     */
    [[nodiscard]] auto to_model_result() const -> model::Result;
};

// ========== Glaze JSON serialization support ==========

// TokenUsage serialization
template <>
struct glz::meta<TokenUsage> {
    using T = TokenUsage;
    static constexpr auto value = glz::object<
        "prompt_tokens", &T::prompt_tokens,
        "completion_tokens", &T::completion_tokens,
        "total_tokens", &T::total_tokens
    >;
};

// LlmResponse serialization
template <>
struct glz::meta<LlmResponse> {
    using T = LlmResponse;
    static constexpr auto value = glz::object<
        "success", &T::success,
        "error_message", &T::error_message,
        "content", &T::content,
        "model", &T::model,
        "finish_reason", &T::finish_reason,
        "usage", &T::usage,
        "latency_ms", &T::latency_ms,
        "request_id", &T::request_id,
        "response_id", &T::response_id
    >;
};

} // namespace moltcat::network
