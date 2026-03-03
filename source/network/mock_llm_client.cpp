#include "mock_llm_client.hpp"
#include "llm_response.hpp"
#include "utils/logger.hpp"
#include <format>
#include <thread>
#include <chrono>

namespace moltcat::network {

MockLlmClient::MockLlmClient(
    uint64_t simulated_latency_ms,
    bool simulate_error
)
    : latency_ms_(simulated_latency_ms)
    , simulate_error_(simulate_error)
    , mock_response_("Mock LLM response for testing")
{
    MOLT_LOGGER.info("Created Mock LLM client: latency={}ms, simulate_error={}",
                     latency_ms_, simulate_error_);
}

auto MockLlmClient::chat_async(
    const LlmRequest& request,
    std::function<void(LlmResponse)> callback
) -> void {
    // Simulate asynchronous call in new thread
    std::thread([this, request, callback]() {
        simulate_async_call(request, callback);
    }).detach();
}

auto MockLlmClient::set_mock_response(std::string response) -> void {
    mock_response_ = std::move(response);
    MOLT_LOGGER.debug("Mock response updated: {}", mock_response_);
}

auto MockLlmClient::set_simulate_error(bool simulate) -> void {
    simulate_error_ = simulate;
    MOLT_LOGGER.debug("Simulate error setting: {}", simulate);
}

auto MockLlmClient::set_latency(uint64_t latency_ms) -> void {
    latency_ms_ = latency_ms;
    MOLT_LOGGER.debug("Simulated latency setting: {}ms", latency_ms_);
}

auto MockLlmClient::simulate_async_call(
    const LlmRequest& request,
    std::function<void(LlmResponse)> callback
) -> void {
    MOLT_LOGGER.info("Mock LLM call started: model={}, messages={}",
                     request.model, request.messages.size());

    auto start_time = std::chrono::steady_clock::now();

    // Simulate API delay
    std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms_));

    auto end_time = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    LlmResponse response;

    if (simulate_error_) {
        // Simulate error response
        response = LlmResponse::error("Mock error: simulated API failure");
        MOLT_LOGGER.warn("Mock LLM call failed: latency={}ms", latency);
    } else {
        // Simulate success response
        response = LlmResponse::ok(mock_response_);
        response.model = request.model;
        response.request_id = request.request_id;
        response.response_id = "mock-response-" + request.request_id;
        response.finish_reason = "stop";
        response.latency_ms = latency;

        // Simulate token usage
        response.usage.prompt_tokens = 50;
        response.usage.completion_tokens = static_cast<uint32_t>(mock_response_.length() / 4);
        response.usage.total_tokens = response.usage.prompt_tokens +
                                       response.usage.completion_tokens;

        MOLT_LOGGER.info("Mock LLM call succeeded: latency={}ms, tokens={}",
                         latency, response.usage.total_tokens);
    }

    // Call callback function
    callback(response);
}

} // namespace moltcat::network
