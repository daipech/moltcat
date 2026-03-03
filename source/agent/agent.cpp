#include "agent.hpp"
#include "utils/logger.hpp"
#include "memory/memory_manager.hpp"
#include "network/llm_request.hpp"
#include "network/llm_response.hpp"
#include "provider/provider_manager.hpp"
#include <algorithm>
#include <format>
#include <thread>
#include <future>

namespace moltcat::agent {

// ========== Constructor and Destructor ==========

Agent::Agent(
    std::string instance_id,
    const AgentTemplate& templ
)
    : instance_id_(std::move(instance_id))
    , template_(templ)
    , config_(templ.get_config())
    , metadata_(templ.get_metadata())
    , runtime_state_()
    , memory_view_(std::make_unique<memory::AgentMemoryView>(
        instance_id_,
        memory::MEMORY_MANAGER
    ))
    , provider_(PROVIDER_MANAGER.get_provider(config_.provider_id))
    , is_running_(false)
{
    if (!provider_) {
        MOLT_LOGGER.error("Failed to create Agent: Provider not found: {}",
                         config_.provider_id);
        throw std::runtime_error("Provider not found: " + config_.provider_id);
    }

    MOLT_LOGGER.info("Created Agent instance (async version): {} (template={}, provider={})",
                     instance_id_, template_.get_template_id(),
                     config_.provider_id);
}

Agent::Agent(
    std::string instance_id,
    const AgentTemplate& templ,
    const model::AgentConfig& config_overrides
)
    : instance_id_(std::move(instance_id))
    , template_(templ)
    , config_(config_overrides)  // Note: using overridden configuration here
    , metadata_(templ.get_metadata())
    , runtime_state_()
    , memory_view_(std::make_unique<memory::AgentMemoryView>(
        instance_id_,
        memory::MEMORY_MANAGER
    ))
    , provider_(PROVIDER_MANAGER.get_provider(config_overrides.provider_id))
    , is_running_(false)
{
    if (!provider_) {
        MOLT_LOGGER.error("Failed to create Agent: Provider not found: {}",
                         config_overrides.provider_id);
        throw std::runtime_error("Provider not found: " + config_overrides.provider_id);
    }

    MOLT_LOGGER.info("Created Agent instance (with config overrides, async version): {} (template={}, provider={})",
                     instance_id_, template_.get_template_id(),
                     config_overrides.provider_id);
}

Agent::~Agent() {
    if (is_running_) {
        MOLT_LOGGER.warn("Agent is still running, stopping: {}", instance_id_);
        stop();
    }

    // Cleanup working memories
    memory::MEMORY_MANAGER.clear_working_memories(instance_id_);

    MOLT_LOGGER.info("Destroyed Agent instance: {}", instance_id_);
}

// ========== Lifecycle Management ==========

auto Agent::start() -> void {
    if (is_running_) {
        MOLT_LOGGER.warn("Agent is already running: {}", instance_id_);
        return;
    }

    MOLT_LOGGER.info("Starting Agent: {}", instance_id_);
    runtime_state_.set_running();
    is_running_ = true;
}

auto Agent::stop() -> void {
    if (!is_running_) {
        MOLT_LOGGER.warn("Agent is not running: {}", instance_id_);
        return;
    }

    MOLT_LOGGER.info("Stopping Agent: {}", instance_id_);
    runtime_state_.set_offline();
    is_running_ = false;
}

auto Agent::restart() -> void {
    MOLT_LOGGER.info("Restarting Agent: {}", instance_id_);
    stop();
    // Brief wait to ensure state fully transitions
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    start();
}

auto Agent::is_available() const noexcept -> bool {
    return is_running_ &&
           runtime_state_.get_state() == AgentState::RUNNING;
}

// ========== Asynchronous Task Execution (Core) ==========

auto Agent::execute_async(
    const model::Task& task,
    std::shared_ptr<messaging::Context<model::Result>> context
) -> void {
    // 1. Check Agent state
    if (!is_running_) {
        MOLT_LOGGER.error("Agent not started, cannot execute task: {}", instance_id_);
        context->set_result(model::Result::error("Agent not running"));
        return;
    }

    // 2. Check rate limiting
    if (!runtime_state_.check_rate_limit(config_.requests_per_minute)) {
        MOLT_LOGGER.warn("Agent rate limit exceeded: {}", instance_id_);
        auto error_result = model::Result::rate_limit_exceeded();
        context->set_result(error_result);
        return;
    }

    // 3. Update state
    runtime_state_.set_running();
    runtime_state_.increment_active_tasks();
    update_request_count();

    MOLT_LOGGER.info("Agent {} started async task execution: {}", instance_id_, task.get_task_id());

    try {
        // 4. Retrieve relevant memories (synchronous, fast)
        auto relevant_memories = retrieve_relevant_memories(task);

        MOLT_LOGGER.debug("Agent {} retrieved {} relevant memories",
                          instance_id_, relevant_memories.size());

        // 5. Add to context memories
        for (const auto& memory : relevant_memories) {
            memory_view_->add_context_memory(memory.memory_id);
        }

        // 6. Call LLM asynchronously (returns immediately, non-blocking)
        call_llm_async(task, relevant_memories, context);

        // 7. Return immediately (non-blocking)
        MOLT_LOGGER.debug("Agent {} async task submitted: {}", instance_id_, task.get_task_id());

    } catch (const std::exception& e) {
        MOLT_LOGGER.error("Async task execution exception: {} (task={}, error={})",
                          instance_id_, task.get_task_id(), e.what());

        auto error_result = model::Result::error(std::string("Exception: ") + e.what());
        context->set_result(error_result);

        // Cleanup
        runtime_state_.decrement_active_tasks();
        if (runtime_state_.get_active_tasks() == 0) {
            runtime_state_.set_idle();
        }
    }
}

auto Agent::execute_async(
    const model::Task& task,
    const model::Context& model_context,
    std::shared_ptr<messaging::Context<model::Result>> context
) -> void {
    // Add model_context information to context (as metadata)
    // Note: new Context does not support direct metadata setting, can add via with_value during creation
    execute_async(task, context);
}

auto Agent::execute_batch_async(
    const std::vector<model::Task>& tasks,
    std::shared_ptr<messaging::Context<model::Result>> context
) -> void {
    MOLT_LOGGER.info("Agent {} batch async task execution: {} tasks", instance_id_, tasks.size());

    // Create sub-context for each task
    for (const auto& task : tasks) {
        auto sub_context = messaging::with_timeout<model::Result>(
            context,
            task.timeout_ms
        );

        // Execute subtask asynchronously (note: simplified here, does not track batch progress)
        execute_async(task, sub_context);
    }

    // Return immediately, non-blocking
}

// ========== Capability Queries ==========

auto Agent::has_capability(std::string_view capability) const -> bool {
    const auto& capabilities = config_.allowed_capabilities;
    return std::find(capabilities.begin(), capabilities.end(), capability)
           != capabilities.end();
}

auto Agent::get_capabilities() const -> std::vector<std::string> {
    return config_.allowed_capabilities;
}

// ========== Memory Operations ==========

auto Agent::store_private_memory(
    const model::Context& model_context,
    std::string_view content,
    float importance
) -> std::string {
    memory::MemoryEntry memory{
        .content = {
            {"context_data", model_context.to_json()},
            {"content", std::string(content)}
        },
        .type = memory::MemoryType::EPISODIC,
        .visibility = memory::MemoryVisibility::PRIVATE,
        .importance = importance,
        .session_id = instance_id_
    };

    return memory_view_->store_private(memory);
}

auto Agent::retrieve_memories(
    std::string_view query,
    size_t top_k
) const -> std::vector<std::string> {
    // Use tag search (simplified implementation)
    auto memories = memory_view_->search_by_tags({std::string(query)});

    std::vector<std::string> contents;
    contents.reserve(std::min(memories.size(), top_k));

    for (size_t i = 0; i < std::min(memories.size(), top_k); ++i) {
        contents.push_back("memory_content_placeholder");
    }

    return contents;
}

auto Agent::get_task_memories(std::string_view task_id) const
    -> std::vector<memory::MemoryEntry> {
    return memory_view_->get_task_memories(task_id);
}

// ========== Asynchronous LLM Call (Core) ==========

auto Agent::call_llm_async(
    const model::Task& task,
    const std::vector<memory::MemoryEntry>& relevant_memories,
    std::shared_ptr<messaging::Context<model::Result>> context
) -> void {
    // Check Provider availability
    if (!provider_) {
        MOLT_LOGGER.error("Provider pointer is null: {}", instance_id_);
        auto error_result = model::Result::error("Provider not initialized");
        context->set_result(error_result);
        return;
    }

    const auto& provider_config = provider_->get_config();

    MOLT_LOGGER.info("Agent {} async LLM call: provider={}, model={}, task={}",
                     instance_id_,
                     provider_config.provider_id,
                     provider_config.default_model,
                     task.get_task_id());

    // Get LLM client connection from Provider
    auto* client = provider_->acquire_client(provider_config.default_model);
    if (!client) {
        MOLT_LOGGER.error("Failed to get connection from Provider: {}",
                         provider_config.provider_id);
        auto error_result = model::Result::error("Failed to get LLM connection");
        context->set_result(error_result);
        return;
    }

    // Build LLM request
    network::LlmRequest request;
    request.model = provider_config.default_model;

    // Add relevant memories as context
    if (!relevant_memories.empty()) {
        std::string context_str = "Relevant memories:\n";
        for (const auto& memory : relevant_memories) {
            context_str += "- " + memory.content.dump() + "\n";
        }
        request.add_message(network::MessageRole::SYSTEM, context_str);
    }

    // Add user task
    request.add_message(network::MessageRole::USER, task.get_prompt());

    // Set parameters (use Provider defaults)
    request.temperature = provider_config.default_temperature;
    request.max_tokens = provider_config.default_max_tokens;
    request.request_id = task.get_task_id();

    // Asynchronous LLM call (set callback)
    client->chat_async(request, [
        this,
        task,
        context,
        client
    ](network::LlmResponse llm_response) mutable {
        // Callback: executed after LLM returns
        // Note: this callback may execute in a background thread

        try {
            // 1. Check if context has been cancelled
            if (context->is_done()) {
                MOLT_LOGGER.warn("Task cancelled, skipping response processing: {}", task.get_task_id());
                provider_->release_client(client);
                return;
            }

            // 2. Convert to Result
            auto result = llm_response.to_model_result();

            // Set metadata
            result.set_meta("agent_id", instance_id_);
            if (provider_) {
                result.set_meta("provider_id", provider_->get_config().provider_id);
                result.set_meta("model", provider_->get_config().default_model);
            }

            // 3. Record episodic memory
            if (result.get_status() == model::ResultStatus::SUCCESS ||
                result.get_status() == model::ResultStatus::PARTIAL) {

                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()
                ).count();

                std::string model_used = provider_ ? provider_->get_config().default_model : "unknown";

                memory::MemoryEntry memory{
                    .content = {
                        {"task_id", task.get_task_id()},
                        {"task_type", static_cast<int>(task.get_type())},
                        {"result_status", static_cast<int>(result.get_status())},
                        {"duration_ms", duration_ms},
                        {"model_used", model_used},
                        {"prompt", task.get_prompt()},
                        {"response", result.get_output()}
                    },
                    .type = memory::MemoryType::EPISODIC,
                    .visibility = memory::MemoryVisibility::PRIVATE,
                    .importance = 0.6f,
                    .task_id = task.get_task_id(),
                    .session_id = instance_id_,
                    .tags = {"task_execution"}
                };

                if (provider_) {
                    memory.tags.push_back(provider_->get_config().provider_id);
                }

                for (const auto& tag : task.get_tags()) {
                    memory.tags.push_back(tag);
                }

                memory_view_->store_private(memory);
            }

            // 4. Clear context memories
            memory_view_->clear_context_memories();

            // 5. Update statistics
            auto duration_ms = result.execution_time_ms;
            runtime_state_.record_execution(duration_ms);
            runtime_state_.decrement_active_tasks();

            if (runtime_state_.get_active_tasks() == 0) {
                runtime_state_.set_idle();
            }

            // 6. Set result to Context
            context->set_result(result);

            MOLT_LOGGER.info("Agent {} completed task: {} (status={})",
                             instance_id_, task.get_task_id(),
                             static_cast<int>(result.get_status()));

        } catch (const std::exception& e) {
            MOLT_LOGGER.error("LLM callback exception: {} (task={}, error={})",
                              instance_id_, task.get_task_id(), e.what());

            auto error_result = model::Result::error(std::string("Exception: ") + e.what());
            context->set_result(error_result);

            runtime_state_.decrement_active_tasks();
            if (runtime_state_.get_active_tasks() == 0) {
                runtime_state_.set_idle();
            }
        }

        // Release connection back to Provider
        if (provider_) {
            provider_->release_client(client);
        }
    });

    // Return immediately, do not wait for LLM response
}

// ========== Helper Methods ==========

auto Agent::retrieve_relevant_memories(
    const model::Task& task
) const -> std::vector<memory::MemoryEntry> {
    // Phase 3: Simplified implementation, retrieve based on task ID and tags
    std::vector<memory::MemoryEntry> relevant_memories;

    // 1. Retrieve historical memories for the same task
    auto task_memories = memory_view_->get_task_memories(task.get_task_id());
    for (auto& memory : task_memories) {
        relevant_memories.push_back(std::move(memory));
    }

    // 2. Retrieve tag-based memories
    const auto& tags = task.get_tags();
    if (!tags.empty()) {
        auto tagged_memories = memory_view_->search_by_tags(tags);
        for (auto& memory : tagged_memories) {
            relevant_memories.push_back(std::move(memory));
        }
    }

    // Limit count (avoid overly long context)
    if (relevant_memories.size() > 10) {
        relevant_memories.resize(10);
    }

    return relevant_memories;
}

auto Agent::update_request_count() -> void {
    runtime_state_.increment_requests_in_current_minute();
}

// ========== A2A 异步任务执行 ==========

auto Agent::execute_a2a_async(
    const A2AMessage& message,
    std::shared_ptr<messaging::Context<model::Result>> context
) -> void {
    MOLT_LOGGER.info("Agent {} 接收 A2A 消息: message_id={}, task_id={}",
                     instance_id_, message.id, message.task.id);

    // 使用 A2AAdapter 将 A2A Message 转换为内部 Task
    A2AAdapter adapter;
    auto model_task = adapter.a2a_message_to_task(message);

    // 调用基础异步执行方法
    execute_async(model_task, context);
}

// ========== 已废弃的方法 ==========

// execute_a2a_with_memory_async 和 execute_a2a_batch_async 已被移除
// 记忆查询现在通过 context 的 with_value 传递
// 批量执行通过 execute_batch_async 处理


} // namespace moltcat::agent
