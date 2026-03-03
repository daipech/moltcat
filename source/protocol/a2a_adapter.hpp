#pragma once

#include "a2a_types.hpp"
#include "../model/task.hpp"
#include "../model/result.hpp"
#include "../model/context.hpp"
#include <string>
#include <optional>
#include <vector>

namespace moltcat::protocol::adapter {

// ================================
// A2A Protocol Adapter
// ================================

/**
 * @brief Adapter from MoltCat Model to A2A protocol
 *
 * Responsible for converting between MoltCat internal types (model::Task, model::Result) and A2A protocol types
 *
 * Design principles:
 * 1. Keep model layer unchanged (backward compatible)
 * 2. Convert at boundaries (Agent execution entry/exit)
 * 3. Extend A2A Message to carry MoltCat-specific information
 */
class A2AAdapter {
public:
    // ========== model::Task -> A2A ==========

    /**
     * @brief Convert model::Task to A2A Message
     *
     * Conversion mapping:
     * - task.id -> message.context_id
     * - task.payload -> message.parts[0].data
     * - task.type -> message.metadata["task_type"]
     * - task.priority -> message.metadata["priority"]
     *
     * @param task MoltCat task
     * @param user_text User input text (optional)
     * @return A2A Message
     */
    [[nodiscard]] static auto task_to_a2a_message(
        const model::Task& task,
        std::string_view user_text = ""
    ) -> a2a::Message;

    /**
     * @brief Create A2A Task (for returning results)
     *
     * @param task_id Task ID
     * @param status A2A task status
     * @param artifacts Artifact list
     * @return A2A Task
     */
    [[nodiscard]] static auto create_a2a_task(
        std::string_view task_id,
        a2a::TaskStatus status,
        const std::vector<a2a::Artifact>& artifacts = {}
    ) -> a2a::Task;

    // ========== A2A -> model::Result ==========

    /**
     * @brief Convert A2A Message to model::Result
     *
     * @param message A2A message
     * @param task_id Associated task ID
     * @param agent_id Agent ID
     * @return model::Result
     */
    [[nodiscard]] static auto a2a_message_to_result(
        const a2a::Message& message,
        std::string_view task_id,
        std::string_view agent_id
    ) -> model::Result;

    /**
     * @brief Convert A2A Task to model::Result
     *
     * @param a2a_task A2A task
     * @param agent_id Agent ID
     * @return model::Result
     */
    [[nodiscard]] static auto a2a_task_to_result(
        const a2a::Task& a2a_task,
        std::string_view agent_id
    ) -> model::Result;

    // ========== Status mapping ==========

    /**
     * @brief Map model::TaskStatus -> A2A TaskState
     */
    [[nodiscard]] static auto map_task_status(model::TaskStatus status)
        -> a2a::TaskState;

    /**
     * @brief Map A2A TaskState -> model::TaskStatus
     */
    [[nodiscard]] static auto map_a2a_task_state(a2a::TaskState state)
        -> model::TaskStatus;

    /**
     * @brief Map model::ResultStatus -> A2A TaskStatus
     */
    [[nodiscard]] static auto map_result_status(model::ResultStatus status)
        -> a2a::TaskStatus;

    // ========== Context adaptation ==========

    /**
     * @brief Create A2A Message from Context
     *
     * @param context MoltCat context
     * @param role Message role
     * @return A2A Message
     */
    [[nodiscard]] static auto context_to_a2a_message(
        const model::Context& context,
        a2a::Role role = a2a::Role::USER
    ) -> a2a::Message;

    // ========== Artifact creation ==========

    /**
     * @brief Create A2A Artifact from model::Result
     *
     * @param result Execution result
     * @param artifact_type Artifact type (text/data/code, etc.)
     * @return A2A Artifact
     */
    [[nodiscard]] static auto result_to_artifact(
        const model::Result& result,
        std::string_view artifact_type = "data"
    ) -> a2a::Artifact;

    /**
     * @brief Create text Artifact
     *
     * @param text Text content
     * @param title Title
     * @return A2A Artifact
     */
    [[nodiscard]] static auto create_text_artifact(
        std::string_view text,
        std::string_view title = ""
    ) -> a2a::Artifact;

    /**
     * @brief Create data Artifact
     *
     * @param data JSON data
     * @param title Title
     * @return A2A Artifact
     */
    [[nodiscard]] static auto create_data_artifact(
        const glz::json_t& data,
        std::string_view title = ""
    ) -> a2a::Artifact;

    // ========== Error handling ==========

    /**
     * @brief Create error A2A Message
     *
     * @param error_code Error code
     * @param error_message Error message
     * @param context_id Context ID
     * @return A2A Message
     */
    [[nodiscard]] static auto create_error_message(
        std::string_view error_code,
        std::string_view error_message,
        std::string_view context_id
    ) -> a2a::Message;

    /**
     * @brief Create failed A2A Task
     *
     * @param task_id Task ID
     * @param error_message Error message
     * @return A2A Task
     */
    [[nodiscard]] static auto create_failed_task(
        std::string_view task_id,
        std::string_view error_message
    ) -> a2a::Task;

    // ========== A2A -> model::Task ==========

    /**
     * @brief Convert A2A Message to model::Task
     *
     * Conversion mapping:
     * - message.context_id -> task.id
     * - message.get_text_content() -> task.description
     * - message.metadata -> task.payload
     * - message.metadata["task_type"] -> task.type
     *
     * @param message A2A message
     * @return model::Task
     */
    [[nodiscard]] static auto a2a_message_to_task(
        const a2a::Message& message
    ) -> model::Task;

    // ========== model::Result -> A2A ==========

    /**
     * @brief Convert model::Result to A2A Task
     *
     * @param original_task Original A2A Task (for preserving context_id, etc.)
     * @param result MoltCat execution result
     * @return A2A Task (updated status and artifacts)
     */
    [[nodiscard]] static auto model_result_to_a2a_task(
        const a2a::Task& original_task,
        const model::Result& result
    ) -> a2a::Task;
};

// ================================
// Inline implementations
// ================================

// model::Task -> A2A Message
inline auto A2AAdapter::task_to_a2a_message(
    const model::Task& task,
    std::string_view user_text
) -> a2a::Message {
    auto msg = a2a::Message::create_user_message(
        user_text.empty() ? task.description : user_text,
        task.id
    );

    // Set task type to metadata
    msg.metadata["task_type"] = task.type;
    msg.metadata["priority"] = static_cast<int>(task.priority);
    msg.metadata["timeout_ms"] = task.timeout_ms;
    msg.metadata["created_at"] = task.created_at;

    // If has payload, add as data part
    if (task.payload.is_object() || task.payload.is_array()) {
        msg.parts.push_back(a2a::Part::from_json(task.payload));
    }

    return msg;
}

// Create A2A Task
inline auto A2AAdapter::create_a2a_task(
    std::string_view task_id,
    a2a::TaskStatus status,
    const std::vector<a2a::Artifact>& artifacts
) -> a2a::Task {
    a2a::Task a2a_task;
    a2a_task.id = std::string(task_id);
    a2a_task.context_id = std::string(task_id);
    a2a_task.status = status;
    a2a_task.artifacts = artifacts;

    return a2a_task;
}

// A2A Message -> model::Result
inline auto A2AAdapter::a2a_message_to_result(
    const a2a::Message& message,
    std::string_view task_id,
    std::string_view agent_id
) -> model::Result {
    model::Result result;
    result.result_id = message.message_id;
    result.task_id = std::string(task_id);
    result.agent_id = std::string(agent_id);
    result.status = model::ResultStatus::SUCCESS;

    // Extract text content
    if (auto text = message.get_text_content()) {
        result.output = *text;
    }

    // Extract metadata
    result.data = message.metadata;

    return result;
}

// A2A Task -> model::Result
inline auto A2AAdapter::a2a_task_to_result(
    const a2a::Task& a2a_task,
    std::string_view agent_id
) -> model::Result {
    model::Result result;
    result.result_id = a2a_task.id;
    result.task_id = a2a_task.id;
    result.agent_id = std::string(agent_id);

    // Map status
    switch (a2a_task.status.state) {
        case a2a::TaskState::COMPLETED:
            result.status = model::ResultStatus::SUCCESS;
            break;
        case a2a::TaskState::FAILED:
            result.status = model::ResultStatus::FAILURE;
            break;
        case a2a::TaskState::CANCELED:
            result.status = model::ResultStatus::CANCELLED;
            break;
        default:
            result.status = model::ResultStatus::PENDING;
            break;
    }

    // Extract artifacts
    if (!a2a_task.artifacts.empty()) {
        // Use data from first artifact as output
        const auto& first_artifact = a2a_task.artifacts[0];
        if (first_artifact.parts.size() > 0 && first_artifact.parts[0].data) {
            result.data = *first_artifact.parts[0].data;
        }
    }

    // Extract metadata
    result.metadata = a2a_task.metadata;

    return result;
}

// Status mapping: model::TaskStatus -> A2A TaskState
inline auto A2AAdapter::map_task_status(model::TaskStatus status)
    -> a2a::TaskState {
    switch (status) {
        case model::TaskStatus::PENDING:
            return a2a::TaskState::SUBMITTED;
        case model::TaskStatus::RUNNING:
            return a2a::TaskState::WORKING;
        case model::TaskStatus::COMPLETED:
            return a2a::TaskState::COMPLETED;
        case model::TaskStatus::FAILED:
            return a2a::TaskState::FAILED;
        case model::TaskStatus::CANCELLED:
            return a2a::TaskState::CANCELED;
        default:
            return a2a::TaskState::UNSPECIFIED;
    }
}

// Status mapping: A2A TaskState -> model::TaskStatus
inline auto A2AAdapter::map_a2a_task_state(a2a::TaskState state)
    -> model::TaskStatus {
    switch (state) {
        case a2a::TaskState::SUBMITTED:
            return model::TaskStatus::PENDING;
        case a2a::TaskState::WORKING:
            return model::TaskStatus::RUNNING;
        case a2a::TaskState::COMPLETED:
            return model::TaskStatus::COMPLETED;
        case a2a::TaskState::FAILED:
            return model::TaskStatus::FAILED;
        case a2a::TaskState::CANCELED:
            return model::TaskStatus::CANCELLED;
        default:
            return model::TaskStatus::PENDING;
    }
}

// Status mapping: model::ResultStatus -> A2A TaskStatus
inline auto A2AAdapter::map_result_status(model::ResultStatus status)
    -> a2a::TaskStatus {
    switch (status) {
        case model::ResultStatus::SUCCESS:
            return a2a::TaskStatus{a2a::TaskState::COMPLETED};
        case model::ResultStatus::FAILURE:
            return a2a::TaskStatus{a2a::TaskState::FAILED};
        case model::ResultStatus::CANCELLED:
            return a2a::TaskStatus{a2a::TaskState::CANCELED};
        default:
            return a2a::TaskStatus{a2a::TaskState::SUBMITTED};
    }
}

// Context -> A2A Message
inline auto A2AAdapter::context_to_a2a_message(
    const model::Context& context,
    a2a::Role role
) -> a2a::Message {
    auto msg = a2a::Message::create();
    msg.role = role;
    msg.context_id = context.context_id;

    // Add text content
    if (!context.get_user_data().is_null() || !context.get_shared_state().is_null()) {
        // Try user_data first, then shared_state
        auto user_data = context.get_user_data();
        if (!user_data.is_null()) {
            if (user_data.is_string()) {
                msg.parts.push_back(a2a::Part::from_text(user_data.get<std::string>()));
            }
        }
    }

    // Copy metadata
    msg.metadata = context.metadata;

    return msg;
}

// result -> artifact
inline auto A2AAdapter::result_to_artifact(
    const model::Result& result,
    std::string_view artifact_type
) -> a2a::Artifact {
    a2a::Artifact artifact;
    artifact.name = std::string(artifact_type);
    if (!result.data.is_null()) {
        artifact.parts.push_back(a2a::Part::from_json(result.data));
    }
    artifact.metadata["result_id"] = result.result_id;
    artifact.metadata["agent_id"] = result.agent_id;
    artifact.metadata["status"] = static_cast<int>(result.status);

    return artifact;
}

// Create text artifact
inline auto A2AAdapter::create_text_artifact(
    std::string_view text,
    std::string_view title
) -> a2a::Artifact {
    a2a::Artifact artifact;
    artifact.artifact_id = utils::UUID::generate_v4();
    artifact.name = title.empty() ? "text" : std::string(title);
    artifact.parts.push_back(a2a::Part::from_text(text));
    artifact.metadata = glz::json_t{};
    artifact.extensions = std::vector<std::string>{};
    return artifact;
}

// Create data artifact
inline auto A2AAdapter::create_data_artifact(
    const glz::json_t& data,
    std::string_view title
) -> a2a::Artifact {
    a2a::Artifact artifact;
    artifact.artifact_id = utils::UUID::generate_v4();
    artifact.name = title.empty() ? "data" : std::string(title);
    artifact.parts.push_back(a2a::Part::from_json(data));
    artifact.metadata = glz::json_t{};
    artifact.extensions = std::vector<std::string>{};
    return artifact;
}

// Create error message
inline auto A2AAdapter::create_error_message(
    std::string_view error_code,
    std::string_view error_message,
    std::string_view context_id
) -> a2a::Message {
    auto msg = a2a::Message::create();
    msg.context_id = std::string(context_id);
    msg.role = a2a::Role::AGENT;
    msg.metadata["error_code"] = std::string(error_code);
    msg.metadata["error_message"] = std::string(error_message);

    return msg;
}

// Create failed task
inline auto A2AAdapter::create_failed_task(
    std::string_view task_id,
    std::string_view error_message
) -> a2a::Task {
    a2a::Task task;
    task.id = std::string(task_id);
    task.context_id = std::string(task_id);
    task.status.state = a2a::TaskState::FAILED;
    task.status.message = a2a::Message::create();
    task.status.message->parts.push_back(a2a::Part::from_text(error_message));
    task.metadata["error"] = std::string(error_message);

    return task;
}

// A2A Message -> model::Task
inline auto A2AAdapter::a2a_message_to_task(
    const a2a::Message& message
) -> model::Task {
    model::Task task;

    // Basic mapping
    task.id = message.context_id;
    task.description = message.get_text_content().value_or("");

    // metadata -> payload
    task.payload = message.metadata;

    // Extract specific fields from metadata
    if (message.metadata.contains("task_type")) {
        auto type_value = message.metadata.at("task_type");
        if (type_value.is_string()) {
            task.type = type_value.get<std::string>();
        }
    }

    if (message.metadata.contains("priority")) {
        auto priority_value = message.metadata.at("priority");
        if (priority_value.is_number()) {
            task.priority = static_cast<model::TaskPriority>(priority_value.get<int>());
        }
    }

    if (message.metadata.contains("timeout_ms")) {
        auto timeout_value = message.metadata.at("timeout_ms");
        if (timeout_value.is_number()) {
            task.timeout_ms = timeout_value.get<uint64_t>();
        }
    }

    return task;
}

// model::Result -> A2A Task
inline auto A2AAdapter::model_result_to_a2a_task(
    const a2a::Task& original_task,
    const model::Result& result
) -> a2a::Task {
    a2a::Task a2a_task = original_task;  // Preserve original information

    // Update status
    a2a_task.status = map_result_status(result.status);

    // Convert result to artifact
    auto artifact = result_to_artifact(result, "data");
    artifact.name = "execution_result";

    // If has text output, add text part
    if (!result.output.empty()) {
        artifact.parts.push_back(a2a::Part::from_text(result.output));
    }

    // Add artifact to task
    a2a_task.artifacts.push_back(artifact);

    // Update metadata
    a2a_task.metadata["result_id"] = result.result_id;
    a2a_task.metadata["agent_id"] = result.agent_id;
    a2a_task.metadata["execution_duration_ms"] = result.execution_duration_ms;

    return a2a_task;
}

} // namespace moltcat::protocol::adapter
