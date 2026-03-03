#include "llm_request.hpp"
#include <format>

namespace moltcat::network {

auto LlmRequest::from_config(const model::AgentConfig& config)
    -> LlmRequest {
    LlmRequest request;

    // Basic configuration
    request.model = config.model_name;

    // Generation parameters
    request.temperature = config.temperature;
    request.max_tokens = config.max_tokens;

    // TODO: top_p can be read from config if AgentConfig adds this field

    return request;
}

auto LlmRequest::add_message(MessageRole role, std::string content) -> void {
    messages.push_back(LlmMessage{role, std::move(content), std::nullopt});
}

auto LlmRequest::add_user_message(std::string content) -> void {
    add_message(MessageRole::USER, std::move(content));
}

auto LlmRequest::add_system_message(std::string content) -> void {
    add_message(MessageRole::SYSTEM, std::move(content));
}

auto LlmRequest::add_assistant_message(std::string content) -> void {
    add_message(MessageRole::ASSISTANT, std::move(content));
}

} // namespace moltcat::network
