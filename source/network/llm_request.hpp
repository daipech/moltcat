#pragma once

#include "model/agent_config.hpp"
#include <glaze/glaze.hpp>
#include <string>
#include <vector>
#include <optional>

namespace moltcat::network {

/**
 * @brief LLM message role
 */
enum class MessageRole : uint8_t {
    SYSTEM,
    USER,
    ASSISTANT
};

/**
 * @brief LLM message
 */
struct LlmMessage {
    MessageRole role;
    std::string content;
    std::optional<std::string> name;  // Optional message name
};

/**
 * @brief LLM API request structure
 *
 * Abstracts API differences between different providers, unified request format
 */
struct LlmRequest {
    // ========== Basic configuration ==========
    std::string model;                       // Model name
    std::vector<LlmMessage> messages;        // Conversation history

    // ========== Generation parameters ==========
    std::optional<float> temperature;        // 0.0-1.0 (default read from config)
    std::optional<float> top_p;              // nucleus sampling
    std::optional<uint32_t> max_tokens;      // Maximum token count

    // ========== Metadata ==========
    std::string request_id;                  // Request ID (for tracking)

    /**
     * @brief Build request from AgentConfig
     *
     * @param config Agent configuration
     * @return LlmRequest Request object
     */
    [[nodiscard]] static auto from_config(const model::AgentConfig& config)
        -> LlmRequest;

    /**
     * @brief Add message
     *
     * @param role Message role
     * @param content Message content
     */
    auto add_message(MessageRole role, std::string content) -> void;

    /**
     * @brief Add user message
     *
     * @param content Message content
     */
    auto add_user_message(std::string content) -> void;

    /**
     * @brief Add system message
     *
     * @param content Message content
     */
    auto add_system_message(std::string content) -> void;

    /**
     * @brief Add assistant message
     *
     * @param content Message content
     */
    auto add_assistant_message(std::string content) -> void;
};

// ========== Glaze JSON serialization support ==========

// MessageRole enum serialization
template <>
struct glz::meta<MessageRole> {
    using T = MessageRole;
    static constexpr auto value = glz::enumerate<"system", "user", "assistant">(
        T::SYSTEM,
        T::USER,
        T::ASSISTANT
    );
};

// LlmMessage serialization
template <>
struct glz::meta<LlmMessage> {
    using T = LlmMessage;
    static constexpr auto value = glz::object<
        "role", &T::role,
        "content", &T::content,
        "name", &T::name
    >;
};

// LlmRequest serialization
template <>
struct glz::meta<LlmRequest> {
    using T = LlmRequest;
    static constexpr auto value = glz::object<
        "model", &T::model,
        "messages", &T::messages,
        "temperature", &T::temperature,
        "top_p", &T::top_p,
        "max_tokens", &T::max_tokens,
        "request_id", &T::request_id
    >;
};

} // namespace moltcat::network
