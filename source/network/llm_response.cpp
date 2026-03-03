#include "llm_response.hpp"
#include "utils/logger.hpp"
#include <format>

namespace moltcat::network {

auto LlmResponse::ok(std::string content) -> LlmResponse {
    LlmResponse response;
    response.success = true;
    response.content = std::move(content);
    return response;
}

auto LlmResponse::error(std::string message) -> LlmResponse {
    LlmResponse response;
    response.success = false;
    response.error_message = std::move(message);
    return response;
}

auto LlmResponse::to_model_result() const -> model::Result {
    if (success) {
        auto result = model::Result::success(content);

        // Set metadata
        if (!model.empty()) {
            result.set_meta("model", model);
        }
        if (!request_id.empty()) {
            result.set_meta("request_id", request_id);
        }
        if (!response_id.empty()) {
            result.set_meta("response_id", response_id);
        }

        // Set usage statistics
        result.set_meta("prompt_tokens", std::to_string(usage.prompt_tokens));
        result.set_meta("completion_tokens", std::to_string(usage.completion_tokens));
        result.set_meta("total_tokens", std::to_string(usage.total_tokens));

        // Set latency
        result.execution_duration_ms = latency_ms;

        return result;
    } else {
        return model::Result::error(error_message);
    }
}

} // namespace moltcat::network
