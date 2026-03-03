#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <glaze/json.hpp>
#include <utils/uuid.hpp>

namespace moltcat::protocol::a2a {

// ================================
// Forward declarations
// ================================

struct Part;
struct Message;
struct TaskStatus;
struct Artifact;
struct Task;

// ================================
// Enumerations
// ================================

/**
 * Task lifecycle state
 *
 * Corresponds to TaskState enum in A2A specification
 */
enum class TaskState : uint32_t {
    UNSPECIFIED = 0,       // Unknown state
    SUBMITTED = 1,         // Submitted
    WORKING = 2,           // Processing
    COMPLETED = 3,         // Completed (terminal state)
    FAILED = 4,            // Failed (terminal state)
    CANCELED = 5,          // Canceled (terminal state)
    INPUT_REQUIRED = 6,    // User input required (interrupted state)
    REJECTED = 7,          // Rejected (terminal state)
    AUTH_REQUIRED = 8,     // Authentication required (interrupted state)
    PARTIALLY_COMPLETED = 9 // Partially completed (intermediate state for parallel/competitive modes)
};

/**
 * Message sender role
 *
 * Corresponds to Role enum in A2A specification
 */
enum class Role : uint32_t {
    UNSPECIFIED = 0,       // Unspecified
    USER = 1,              // Sent by user
    AGENT = 2              // Sent by Agent
};

// ================================
// Part (Message content unit)
// ================================

/**
 * A2A Part - Multimodal container for message content
 *
 * Supports:
 * - text: Plain text content
 * - raw: Raw bytes (base64 encoded)
 * - url: Resource URL reference
 * - data: Structured JSON data
 */
struct Part {
    // Content (choose one)
    std::optional<std::string> text;                    // Text content
    std::optional<std::vector<uint8_t>> raw;           // Raw bytes
    std::optional<std::string> url;                    // URL reference
    std::optional<glz::json_t> data;                   // JSON data

    // Metadata
    glz::json_t metadata;                              // Extended metadata
    std::optional<std::string> filename;               // Filename (if applicable)
    std::string media_type = "text/plain";             // MIME type

    // Helper constructor methods
    static auto from_text(std::string_view content) -> Part {
        Part part;
        part.text = std::string(content);
        part.media_type = "text/plain";
        return part;
    }

    static auto from_json(const glz::json_t& json_data) -> Part {
        Part part;
        part.data = json_data;
        part.media_type = "application/json";
        return part;
    }

    static auto from_url(std::string_view url, std::string_view mime_type = "application/octet-stream") -> Part {
        Part part;
        part.url = std::string(url);
        part.media_type = std::string(mime_type);
        return part;
    }

    static auto from_raw(const std::vector<uint8_t>& bytes, std::string_view mime_type = "application/octet-stream") -> Part {
        Part part;
        part.raw = bytes;
        part.media_type = std::string(mime_type);
        return part;
    }
};

// Glaze 序列化支持
template <>
struct glz::meta<Part> {
    using T = Part;
    static constexpr auto value = glz::object(
        "text", &T::text,
        "raw", &T::raw,
        "url", &T::url,
        "data", &T::data,
        "metadata", &T::metadata,
        "filename", &T::filename,
        "media_type", &T::media_type
    );
};

// ================================
// Message (A2A Message)
// ================================

/**
 * A2A Message - Basic unit of Agent communication
 *
 * Corresponds to Message structure in A2A specification
 */
struct Message {
    // Required fields
    std::string message_id;                           // UUID (unique message identifier)

    // Optional fields
    std::string context_id;                           // Session context ID (for correlating messages)
    std::optional<std::string> task_id;              // Associated task ID (optional)

    // Message properties
    Role role = Role::USER;                           // Sender role
    std::vector<Part> parts;                          // Message content (multimodal)

    // Extended fields
    glz::json_t metadata;                             // Flexible metadata
    std::vector<std::string> extensions;             // Extension URI list
    std::vector<std::string> reference_task_ids;     // Referenced task ID list

    // Helper method: Create new message
    static auto create() -> Message {
        Message msg;
        msg.message_id = utils::UUID::generate_v4();
        msg.role = Role::USER;
        msg.parts = std::vector<Part>{};
        msg.metadata = glz::json_t{};
        msg.extensions = std::vector<std::string>{};
        msg.reference_task_ids = std::vector<std::string>{};
        return msg;
    }

    // Helper method: Create user text message
    static auto create_user_message(std::string_view text, std::string_view context_id = "") -> Message {
        Message msg = create();
        msg.role = Role::USER;
        msg.context_id = std::string(context_id);
        msg.parts.push_back(Part::from_text(text));
        return msg;
    }

    // Helper method: Create Agent response message
    static auto create_agent_message(std::string_view text, std::string_view context_id, std::string_view task_id) -> Message {
        Message msg = create();
        msg.role = Role::AGENT;
        msg.context_id = std::string(context_id);
        msg.task_id = std::string(task_id);
        msg.parts.push_back(Part::from_text(text));
        return msg;
    }

    // Serialize to JSON
    auto serialize() const -> std::string {
        std::string json;
        auto error = glz::write_json(*this, json);
        if (error) {
            throw std::runtime_error("Message serialization failed: " + std::string(error));
        }
        return json;
    }

    // Deserialize from JSON
    static auto deserialize(const std::string& json) -> std::optional<Message> {
        Message msg;
        auto error = glz::read_json(msg, json);
        if (error) {
            return std::nullopt;
        }
        return msg;
    }

    // Get main text content (convenience method)
    auto get_text_content() const -> std::optional<std::string> {
        if (parts.empty()) {
            return std::nullopt;
        }
        return parts[0].text;
    }
};

// Glaze 序列化支持
template <>
struct glz::meta<Message> {
    using T = Message;
    static constexpr auto value = glz::object(
        "message_id", &T::message_id,
        "context_id", &T::context_id,
        "task_id", &T::task_id,
        "role", &T::role,
        "parts", &T::parts,
        "metadata", &T::metadata,
        "extensions", &T::extensions,
        "reference_task_ids", &T::reference_task_ids
    );
};

// ================================
// TaskStatus (Task status)
// ================================

/**
 * A2A TaskStatus - Current task status
 *
 * Corresponds to TaskStatus structure in A2A specification
 */
struct TaskStatus {
    TaskState state = TaskState::UNSPECIFIED;        // Current state
    std::optional<Message> message;                  // Status-associated message
    std::string timestamp;                           // ISO 8601 timestamp

    // Helper method: Create initial status
    static auto create_submitted() -> TaskStatus {
        return TaskStatus{
            .state = TaskState::SUBMITTED,
            .message = std::nullopt,
            .timestamp = get_iso8601_timestamp()
        };
    }

    // Helper method: Update to working
    auto set_working(const Message& msg) -> void {
        state = TaskState::WORKING;
        message = msg;
        timestamp = get_iso8601_timestamp();
    }

    // Helper method: Mark as completed
    auto set_completed(const std::vector<Artifact>& artifacts = {}) -> void {
        state = TaskState::COMPLETED;
        timestamp = get_iso8601_timestamp();
    }

    // Helper method: Mark as failed
    auto set_failed(std::string_view error_msg) -> void {
        state = TaskState::FAILED;
        if (!message) {
            message = Message::create();
        }
        message->parts.push_back(Part::from_text(error_msg));
        timestamp = get_iso8601_timestamp();
    }

    // Check if terminal state
    auto is_terminal() const -> bool {
        return state == TaskState::COMPLETED
            || state == TaskState::FAILED
            || state == TaskState::CANCELED
            || state == TaskState::REJECTED;
    }

    // Check if intermediate state (non-terminal)
    auto is_intermediate() const -> bool {
        return state == TaskState::SUBMITTED
            || state == TaskState::WORKING
            || state == TaskState::PARTIALLY_COMPLETED;
    }

private:
    // Get ISO 8601 timestamp
    static auto get_iso8601_timestamp() -> std::string {
        // TODO: Implement timestamp generation
        // Return format: 2023-10-27T10:00:00Z
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();

        // Simplified implementation: return millisecond timestamp directly
        return std::to_string(ms);
    }
};

// Glaze 序列化支持
template <>
struct glz::meta<TaskStatus> {
    using T = TaskStatus;
    static constexpr auto value = glz::object(
        "state", &T::state,
        "message", &T::message,
        "timestamp", &T::timestamp
    );
};

// ================================
// Artifact (Task output)
// ================================

/**
 * A2A Artifact - Task output result
 *
 * Corresponds to Artifact structure in A2A specification
 */
struct Artifact {
    // Required fields
    std::string artifact_id;                          // UUID (artifact unique identifier)
    std::string name;                                 // Name
    std::vector<Part> parts;                          // Content parts (at least one)

    // Optional fields
    std::optional<std::string> description;          // Description
    glz::json_t metadata;                             // Metadata
    std::vector<std::string> extensions;             // Extension URI list

    // Helper method: Create result Artifact
    static auto create_result(std::string_view result_text) -> Artifact {
        Artifact artifact;
        artifact.artifact_id = utils::UUID::generate_v4();
        artifact.name = "result";
        artifact.parts.push_back(Part::from_text(result_text));
        artifact.metadata = glz::json_t{};
        artifact.extensions = std::vector<std::string>{};
        return artifact;
    }

    // Helper method: Create error Artifact
    static auto create_error(std::string_view error_msg) -> Artifact {
        Artifact artifact;
        artifact.artifact_id = utils::UUID::generate_v4();
        artifact.name = "error";
        artifact.description = "Task execution failed";
        artifact.parts.push_back(Part::from_text(error_msg));
        artifact.metadata["success"] = false;
        artifact.extensions = std::vector<std::string>{};
        return artifact;
    }
};

// Glaze 序列化支持
template <>
struct glz::meta<Artifact> {
    using T = Artifact;
    static constexpr auto value = glz::object(
        "artifact_id", &T::artifact_id,
        "name", &T::name,
        "description", &T::description,
        "parts", &T::parts,
        "metadata", &T::metadata,
        "extensions", &T::extensions
    );
};

// ================================
// Task (A2A Task)
// ================================

/**
 * A2A Task - Core unit of Agent execution
 *
 * Corresponds to Task structure in A2A specification
 *
 * MoltCat extension fields:
 * - task_type: Task type (original Task.type)
 * - priority: Priority (0-9, 9 highest)
 * - timeout_ms: Timeout (milliseconds)
 */
struct Task {
    // ========== A2A standard fields ==========

    // Required fields
    std::string id;                                   // UUID (task unique identifier)
    std::string context_id;                           // Session context ID
    TaskStatus status;                                // Current status

    // Optional fields
    std::vector<Artifact> artifacts;                  // Output result list
    std::vector<Message> history;                     // Message history
    glz::json_t metadata;                             // Custom metadata

    // ========== MoltCat extension fields ==========

    std::string task_type;                            // Task type (e.g., "code_review")
    int priority = 5;                                 // Priority (0-9)
    uint64_t timeout_ms = 30000;                      // Timeout (milliseconds, default 30s)

    // Convenience output fields (for quick access, actually stored in metadata)
    std::string output;                               // Task output result
    std::string error_message;                        // Error message

    // Helper method: Create new task
    static auto create(std::string_view type, const Message& user_msg) -> Task {
        Task task;
        task.id = utils::UUID::generate_v4();
        task.context_id = user_msg.context_id.empty()
            ? utils::UUID::generate_v4()
            : user_msg.context_id;
        task.status = TaskStatus::create_submitted();
        task.history.push_back(user_msg);
        task.artifacts = std::vector<Artifact>{};
        task.metadata = glz::json_t{};
        task.task_type = type;
        task.priority = 5;
        task.timeout_ms = 30000;
        return task;
    }

    // Helper method: Add message to history
    auto add_message(const Message& msg) -> void {
        history.push_back(msg);
        if (msg.task_id) {
            // Update task_id association
            if (!msg.task_id || msg.task_id->empty()) {
                const_cast<Message&>(msg).task_id = id;
            }
        }
    }

    // Helper method: Add output Artifact
    auto add_artifact(const Artifact& artifact) -> void {
        artifacts.push_back(artifact);
    }

    // Helper method: Get last user message
    auto get_last_user_message() const -> std::optional<Message> {
        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            if (it->role == Role::USER) {
                return *it;
            }
        }
        return std::nullopt;
    }

    // Helper method: Check if terminal state
    auto is_terminal_state() const -> bool {
        return status.is_terminal();
    }

    // Serialize to JSON
    auto serialize() const -> std::string {
        std::string json;
        auto error = glz::write_json(*this, json);
        if (error) {
            throw std::runtime_error("Task serialization failed: " + std::string(error));
        }
        return json;
    }

    // Deserialize from JSON
    static auto deserialize(const std::string& json) -> std::optional<Task> {
        Task task;
        auto error = glz::read_json(task, json);
        if (error) {
            return std::nullopt;
        }
        return task;
    }
};

// Glaze 序列化支持
template <>
struct glz::meta<Task> {
    using T = Task;
    static constexpr auto value = glz::object(
        // A2A 标准字段
        "id", &T::id,
        "context_id", &T::context_id,
        "status", &T::status,
        "artifacts", &T::artifacts,
        "history", &T::history,
        "metadata", &T::metadata,
        // MoltCat 扩展字段
        "task_type", &T::task_type,
        "priority", &T::priority,
        "timeout_ms", &T::timeout_ms,
        "output", &T::output,
        "error_message", &T::error_message
    );
};

// ================================
// Helper functions
// ================================

/**
 * Generate ISO 8601 timestamp
 *
 * Return format: 2023-10-27T10:00:00Z
 */
inline auto get_iso8601_timestamp() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    // Simplified implementation: return Unix millisecond timestamp
    // TODO: Implement full ISO 8601 formatting
    return std::to_string(ms);
}

/**
 * Convert TaskState to string
 */
inline auto task_state_to_string(TaskState state) -> std::string_view {
    switch (state) {
        case TaskState::UNSPECIFIED:     return "TASK_STATE_UNSPECIFIED";
        case TaskState::SUBMITTED:       return "TASK_STATE_SUBMITTED";
        case TaskState::WORKING:         return "TASK_STATE_WORKING";
        case TaskState::COMPLETED:       return "TASK_STATE_COMPLETED";
        case TaskState::FAILED:          return "TASK_STATE_FAILED";
        case TaskState::CANCELED:        return "TASK_STATE_CANCELED";
        case TaskState::INPUT_REQUIRED:  return "TASK_STATE_INPUT_REQUIRED";
        case TaskState::REJECTED:        return "TASK_STATE_REJECTED";
        case TaskState::AUTH_REQUIRED:   return "TASK_STATE_AUTH_REQUIRED";
        case TaskState::PARTIALLY_COMPLETED: return "TASK_STATE_PARTIALLY_COMPLETED";
        default:                         return "UNKNOWN";
    }
}

/**
 * Convert Role to string
 */
inline auto role_to_string(Role role) -> std::string_view {
    switch (role) {
        case Role::UNSPECIFIED:  return "ROLE_UNSPECIFIED";
        case Role::USER:         return "ROLE_USER";
        case Role::AGENT:        return "ROLE_AGENT";
        default:                 return "UNKNOWN";
    }
}

} // namespace moltcat::protocol::a2a
