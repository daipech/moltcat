#pragma once

#include <cstdint>

namespace moltcat::model {

/**
 * @brief Task priority
 */
enum class TaskPriority : uint8_t {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * @brief Task status
 */
enum class TaskStatus : uint8_t {
    PENDING,        // Waiting to be processed
    DISPATCHED,     // Dispatched to executor
    RUNNING,        // Currently executing
    COMPLETED,      // Successfully finished
    FAILED,         // Execution failed
    TIMEOUT,        // Execution timed out
    CANCELLED       // Cancelled by user
};

/**
 * @brief Result status
 */
enum class ResultStatus : uint8_t {
    SUCCESS,        // Success
    FAILED,         // Failed
    PARTIAL,        // Partial success
    TIMEOUT,        // Timed out
    ERROR           // Error
};

/**
 * @brief Agent state
 */
enum class AgentState : uint8_t {
    IDLE,           // Idle
    BUSY,           // Busy
    ERROR,          // Error
    OFFLINE,        // Offline
    RETIRED         // Retired
};

/**
 * @brief System state
 */
enum class SystemState : uint8_t {
    INITIALIZING,   // Initializing
    RUNNING,        // Running
    PAUSED,         // Paused
    SHUTTING_DOWN,  // Shutting down
    STOPPED         // Stopped
};

/**
 * @brief Resource state
 */
enum class ResourceState : uint8_t {
    AVAILABLE,      // Available
    BUSY,           // Busy
    OVERLOADED,     // Overloaded
    ERROR           // Error
};

/**
 * @brief Convert task priority to string
 */
[[nodiscard]] constexpr auto to_string(TaskPriority priority) -> const char* {
    switch (priority) {
        case TaskPriority::LOW:      return "LOW";
        case TaskPriority::NORMAL:   return "NORMAL";
        case TaskPriority::HIGH:     return "HIGH";
        case TaskPriority::CRITICAL: return "CRITICAL";
        default:                     return "UNKNOWN";
    }
}

/**
 * @brief Convert task status to string
 */
[[nodiscard]] constexpr auto to_string(TaskStatus status) -> const char* {
    switch (status) {
        case TaskStatus::PENDING:    return "PENDING";
        case TaskStatus::DISPATCHED: return "DISPATCHED";
        case TaskStatus::RUNNING:    return "RUNNING";
        case TaskStatus::COMPLETED:  return "COMPLETED";
        case TaskStatus::FAILED:     return "FAILED";
        case TaskStatus::TIMEOUT:    return "TIMEOUT";
        case TaskStatus::CANCELLED:  return "CANCELLED";
        default:                     return "UNKNOWN";
    }
}

/**
 * @brief Convert result status to string
 */
[[nodiscard]] constexpr auto to_string(ResultStatus status) -> const char* {
    switch (status) {
        case ResultStatus::SUCCESS:  return "SUCCESS";
        case ResultStatus::FAILED:   return "FAILED";
        case ResultStatus::PARTIAL:  return "PARTIAL";
        case ResultStatus::TIMEOUT:  return "TIMEOUT";
        case ResultStatus::ERROR:    return "ERROR";
        default:                     return "UNKNOWN";
    }
}

/**
 * @brief Convert agent state to string
 */
[[nodiscard]] constexpr auto to_string(AgentState state) -> const char* {
    switch (state) {
        case AgentState::IDLE:    return "IDLE";
        case AgentState::BUSY:    return "BUSY";
        case AgentState::ERROR:   return "ERROR";
        case AgentState::OFFLINE: return "OFFLINE";
        case AgentState::RETIRED: return "RETIRED";
        default:                  return "UNKNOWN";
    }
}

/**
 * @brief Convert system state to string
 */
[[nodiscard]] constexpr auto to_string(SystemState state) -> const char* {
    switch (state) {
        case SystemState::INITIALIZING:  return "INITIALIZING";
        case SystemState::RUNNING:       return "RUNNING";
        case SystemState::PAUSED:        return "PAUSED";
        case SystemState::SHUTTING_DOWN: return "SHUTTING_DOWN";
        case SystemState::STOPPED:       return "STOPPED";
        default:                         return "UNKNOWN";
    }
}

/**
 * @brief Convert resource state to string
 */
[[nodiscard]] constexpr auto to_string(ResourceState state) -> const char* {
    switch (state) {
        case ResourceState::AVAILABLE:  return "AVAILABLE";
        case ResourceState::BUSY:       return "BUSY";
        case ResourceState::OVERLOADED: return "OVERLOADED";
        case ResourceState::ERROR:      return "ERROR";
        default:                        return "UNKNOWN";
    }
}

} // namespace moltcat::model
