/**
 * @file molt_model.hpp
 * @brief MoltCat data model interfaces
 * @note This file is for plugin developers, defining core data structures
 * @warning Uses plugin interface types to ensure ABI compatibility across DLL boundaries
 */

#pragma once

#include "molt_plugin.hpp"
#include <cstdint>

namespace moltcat {

// ============================================================
// Task related types
// ============================================================

/**
 * @brief Task priority
 */
enum class TaskPriority : int {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3,
};

/**
 * @brief Task status
 */
enum class TaskStatus : uint8_t {
    PENDING,     // Waiting for execution
    RUNNING,     // Executing
    COMPLETED,   // Completed
    FAILED,      // Failed
    CANCELLED,   // Cancelled
    TIMEOUT,     // Timeout
};

// ============================================================
// Timestamp interface
// ============================================================

/**
 * @brief Timestamp interface
 *
 * Time representation across DLL boundaries, avoiding std::chrono ABI issues.
 */
class ITimestamp {
public:
    /**
     * @brief Get Unix timestamp in milliseconds
     */
    virtual auto to_millis() const noexcept -> int64_t = 0;

    /**
     * @brief Create current timestamp
     */
    static auto now() -> ITimestamp*;

    /**
     * @brief Create timestamp from milliseconds
     */
    static auto from_millis(int64_t millis) -> ITimestamp*;

    /**
     * @brief Destroy timestamp
     */
    virtual void destroy() noexcept = 0;

protected:
    virtual ~ITimestamp() = default;
};

// ============================================================
// Task data structure
// ============================================================

/**
 * @brief Task data structure
 *
 * Plugin developers: This structure defines the basic properties of a task,
 * skill plugins receive and process such tasks.
 *
 * @note Uses IString* instead of std::string to ensure ABI compatibility
 *
 * @example
 * @code
 * // Access task properties
 * const char* task_id = task.id->c_str();
 * const char* payload = task.payload->c_str();
 *
 * // No need to free, framework manages lifecycle
 * @endcode
 */
struct MoltTask {
    IString* id = nullptr;                   // Task unique ID (framework manages lifecycle)
    IString* type = nullptr;                 // Task type (framework manages lifecycle)
    IString* payload = nullptr;              // Task payload data (framework manages lifecycle)
    TaskPriority priority = TaskPriority::NORMAL;
    uint64_t timeout_ms = 30000;             // Timeout in milliseconds
    ITimestamp* created_at = nullptr;        // Creation time (framework manages lifecycle)
    IString* parent_id = nullptr;            // Parent task ID (framework manages lifecycle, can be nullptr)
    IString* correlation_id = nullptr;       // Correlation ID (framework manages lifecycle, can be nullptr)

    // Convenience access methods
    auto get_id() const noexcept -> const char* {
        return id ? id->c_str() : "";
    }

    auto get_type() const noexcept -> const char* {
        return type ? type->c_str() : "";
    }

    auto get_payload() const noexcept -> const char* {
        return payload ? payload->c_str() : "";
    }

    auto get_parent_id() const noexcept -> const char* {
        return parent_id ? parent_id->c_str() : "";
    }

    auto get_correlation_id() const noexcept -> const char* {
        return correlation_id ? correlation_id->c_str() : "";
    }
};

// ============================================================
// Execution result
// ============================================================

/**
 * @brief Execution result
 *
 * @note Uses IString* instead of std::string to ensure ABI compatibility
 *       Caller is responsible for freeing returned string pointers
 */
struct MoltResult {
    bool success = false;                    // Execution success
    IString* data = nullptr;                 // Result data (caller must free)
    IString* error_message = nullptr;        // Error message (caller must free)
    uint64_t execution_time_ms = 0;          // Execution time
    TaskStatus status = TaskStatus::COMPLETED;

    /**
     * @brief Create success result
     */
    static auto create_success(IString* data, uint64_t exec_time = 0) -> MoltResult {
        return MoltResult{true, data, nullptr, exec_time, TaskStatus::COMPLETED};
    }

    /**
     * @brief Create failure result
     */
    static auto create_error(IString* error_msg, TaskStatus st = TaskStatus::FAILED) -> MoltResult {
        return MoltResult{false, nullptr, error_msg, 0, st};
    }

    /**
     * @brief Get result data as C string
     */
    auto get_data() const noexcept -> const char* {
        return data ? data->c_str() : "";
    }

    /**
     * @brief Get error message as C string
     */
    auto get_error() const noexcept -> const char* {
        return error_message ? error_message->c_str() : "";
    }

    /**
     * @brief Clean up strings in result
     * @note After call, string pointers become nullptr
     */
    void cleanup() noexcept {
        if (data) {
            data->destroy();
            data = nullptr;
        }
        if (error_message) {
            error_message->destroy();
            error_message = nullptr;
        }
    }
};

// ============================================================
// Execution context
// ============================================================

/**
 * @brief Configuration accessor interface
 *
 * Used for retrieving configuration values from context.
 */
class IConfigAccessor {
public:
    /**
     * @brief Get string configuration
     * @param key Configuration key
     * @param default_value Default value (can be nullptr)
     * @return String copy of configuration value, caller must free
     */
    virtual auto get_string(const char* key, const char* default_value) -> IString* = 0;

    /**
     * @brief Get integer configuration
     */
    virtual auto get_int(const char* key, int64_t default_value) -> int64_t = 0;

    /**
     * @brief Get double configuration
     */
    virtual auto get_double(const char* key, double default_value) -> double = 0;

    /**
     * @brief Get boolean configuration
     */
    virtual auto get_bool(const char* key, bool default_value) -> bool = 0;

protected:
    virtual ~IConfigAccessor() = default;
};

/**
 * @brief State storage interface
 *
 * Used for storing and retrieving shared state during task execution.
 */
class IStateStore {
public:
    /**
     * @brief Set state value
     * @param key State key
     * @param value State value (nullptr means delete)
     */
    virtual void set(const char* key, const char* value) = 0;

    /**
     * @brief Get state value
     * @param key State key
     * @param default_value Default value (can be nullptr)
     * @return String copy of state value, caller must free
     */
    virtual auto get(const char* key, const char* default_value) -> IString* = 0;

    /**
     * @brief Check if key exists
     */
    virtual auto has(const char* key) -> bool = 0;

    /**
     * @brief Remove state value
     */
    virtual void remove(const char* key) = 0;

    /**
     * @brief Clear all state
     */
    virtual void clear() = 0;

protected:
    virtual ~IStateStore() = default;
};

/**
 * @brief Execution context
 *
 * Plugin developers: This structure is passed to skill plugins during task execution,
 * providing runtime environment and shared state.
 *
 * @note All pointers are managed by framework, plugins must not free
 *
 * @example
 * @code
 * // Access context properties
 * const char* agent_id = ctx.agent_id->c_str();
 *
 * // Get configuration
 * int timeout = ctx.config->get_int("timeout", 30000);
 *
 * // Set and get state
 * ctx.state->set("progress", "50");
 * IString* progress = ctx.state->get("progress", "0");
 * // ... use progress
 * progress->destroy();
 * @endcode
 */
struct MoltContext {
    IString* agent_id = nullptr;             // Agent ID executing this task (framework managed)
    IString* task_id = nullptr;              // Current task ID (framework managed)
    IStateStore* state = nullptr;            // Shared state storage (framework managed)
    IConfigAccessor* config = nullptr;       // Configuration accessor (framework managed)
    IString* user_data = nullptr;            // User custom data (framework managed)
    uint64_t attempt_number = 0;             // Current retry count

    // Convenience access methods
    auto get_agent_id() const noexcept -> const char* {
        return agent_id ? agent_id->c_str() : "";
    }

    auto get_task_id() const noexcept -> const char* {
        return task_id ? task_id->c_str() : "";
    }

    auto get_user_data() const noexcept -> const char* {
        return user_data ? user_data->c_str() : "";
    }

    // Convenience config access
    auto get_config_string(const char* key, const char* default_value = "") -> IString* {
        return config ? config->get_string(key, default_value) : IString::create(default_value);
    }

    auto get_config_int(const char* key, int64_t default_value = 0) -> int64_t {
        return config ? config->get_int(key, default_value) : default_value;
    }

    auto get_config_double(const char* key, double default_value = 0.0) -> double {
        return config ? config->get_double(key, default_value) : default_value;
    }

    auto get_config_bool(const char* key, bool default_value = false) -> bool {
        return config ? config->get_bool(key, default_value) : default_value;
    }

    // Convenience state operations
    void set_state(const char* key, const char* value) {
        if (state) state->set(key, value);
    }

    auto get_state(const char* key, const char* default_value = "") -> IString* {
        return state ? state->get(key, default_value) : IString::create(default_value);
    }

    auto has_state(const char* key) -> bool {
        return state ? state->has(key) : false;
    }
};

// ============================================================
// Agent related types
// ============================================================

/**
 * @brief Agent state
 */
enum class AgentState : uint8_t {
    IDLE,        // Idle
    BUSY,        // Busy
    OFFLINE,     // Offline
    ERROR,       // Error
};

/**
 * @brief Agent capability description
 */
struct AgentCapability {
    IString* name = nullptr;             // Capability name (framework managed)
    IString* description = nullptr;      // Capability description (framework managed)
};

/**
 * @brief Agent information
 *
 * @note All IString* lifecycle managed by framework
 */
struct AgentInfo {
    IString* id = nullptr;               // Agent ID (framework managed)
    IString* name = nullptr;             // Agent name (framework managed)
    IString* type = nullptr;             // Agent type (framework managed)
    AgentState state = AgentState::OFFLINE;
    double load_factor = 0.0;            // Load factor [0.0, 1.0]
    uint64_t completed_tasks = 0;
    uint64_t failed_tasks = 0;
    uint64_t total_execution_time_ms = 0;

    // Convenience access
    auto get_id() const noexcept -> const char* {
        return id ? id->c_str() : "";
    }

    auto get_name() const noexcept -> const char* {
        return name ? name->c_str() : "";
    }

    auto get_type() const noexcept -> const char* {
        return type ? type->c_str() : "";
    }
};

// ============================================================
// Event related types
// ============================================================

/**
 * @brief Event type base class
 */
class IEvent {
public:
    virtual ~IEvent() = default;

    /**
     * @brief Get event type identifier
     */
    virtual auto get_type() const -> const char* = 0;

    /**
     * @brief Get event timestamp
     */
    virtual auto get_timestamp() const -> ITimestamp* = 0;

    /**
     * @brief Clone event
     */
    virtual auto clone() const -> IEvent* = 0;

    /**
     * @brief Destroy event
     */
    virtual void destroy() = 0;
};

/**
 * @brief Task event
 */
struct TaskEvent : public IEvent {
    static const char* TYPE;  // "task"

    IString* task_id = nullptr;           // Task ID (framework managed)
    IString* agent_id = nullptr;          // Agent ID (framework managed)
    TaskStatus status = TaskStatus::PENDING;
    IString* message = nullptr;           // Additional message (framework managed)

    auto get_type() const -> const char* override { return TYPE; }
    auto get_timestamp() const -> ITimestamp* override;
    auto clone() const -> IEvent* override;
    void destroy() override;
};

/**
 * @brief Agent event
 */
struct AgentEvent : public IEvent {
    static const char* TYPE;  // "agent"

    IString* agent_id = nullptr;          // Agent ID (framework managed)
    AgentState state = AgentState::OFFLINE;
    IString* message = nullptr;           // Additional message (framework managed)

    auto get_type() const -> const char* override { return TYPE; }
    auto get_timestamp() const -> ITimestamp* override;
    auto clone() const -> IEvent* override;
    void destroy() override;
};

} // namespace moltcat
