#pragma once

#include "types.hpp"
#include <glaze/json.hpp>
#include <string>
#include <cstdint>
#include <cstddef>

namespace moltcat::model {

/**
 * @brief State snapshot
 *
 * Used to save system state at a specific moment
 */
struct StateSnapshot {
    // ========== Time Information ==========
    uint64_t timestamp{0};                       // Snapshot timestamp

    // ========== System State ==========
    SystemState system_state{SystemState::STOPPED};
    uint32_t total_agents{0};
    uint32_t active_agents{0};
    uint32_t idle_agents{0};
    uint32_t error_agents{0};

    // ========== Task Statistics ==========
    uint64_t total_tasks{0};
    uint64_t pending_tasks{0};
    uint64_t running_tasks{0};
    uint64_t completed_tasks{0};
    uint64_t failed_tasks{0};

    // ========== Resource Usage ==========
    double cpu_usage_percent{0.0};               // CPU usage
    double memory_usage_percent{0.0};            // Memory usage
    uint64_t available_memory_bytes{0};          // Available memory

    // ========== Queue Statistics ==========
    size_t pending_queue_size{0};                // Pending queue size
    size_t running_queue_size{0};                // Running queue size
    size_t completed_queue_size{0};              // Completed queue size

    // ========== Extended Fields ==========
    glz::json_t metadata;                        // Extended metadata
};

/**
 * @brief State manager
 *
 * Responsible for state creation, updates, queries, and persistence
 */
class StateManager {
public:
    /**
     * @brief Create state snapshot
     */
    [[nodiscard]] static auto create_snapshot() -> StateSnapshot;

    /**
     * @brief Serialize state snapshot
     */
    [[nodiscard]] static auto serialize_snapshot(const StateSnapshot& snapshot)
        -> std::string;

    /**
     * @brief Deserialize state snapshot
     */
    [[nodiscard]] static auto deserialize_snapshot(std::string_view json_str)
        -> StateSnapshot;

    /**
     * @brief Update system state
     */
    static auto update_system_state(StateSnapshot& snapshot, SystemState new_state) -> void;

    /**
     * @brief Increment task count
     */
    static auto increment_task_count(StateSnapshot& snapshot, TaskStatus status) -> void;

    /**
     * @brief Update agent statistics
     */
    static auto update_agent_stats(StateSnapshot& snapshot,
                                   uint32_t total,
                                   uint32_t active,
                                   uint32_t idle,
                                   uint32_t error) -> void;

    /**
     * @brief Update resource usage
     */
    static auto update_resource_usage(StateSnapshot& snapshot,
                                      double cpu_percent,
                                      double memory_percent,
                                      uint64_t available_memory) -> void;
};

} // namespace moltcat::model

// Glaze serialization support - StateSnapshot
template <>
struct glz::meta<moltcat::model::StateSnapshot> {
    using T = moltcat::model::StateSnapshot;
    static constexpr auto value = glz::object<
        "timestamp",             &T::timestamp,
        "system_state",          &T::system_state,
        "total_agents",          &T::total_agents,
        "active_agents",         &T::active_agents,
        "idle_agents",           &T::idle_agents,
        "error_agents",          &T::error_agents,
        "total_tasks",           &T::total_tasks,
        "pending_tasks",         &T::pending_tasks,
        "running_tasks",         &T::running_tasks,
        "completed_tasks",       &T::completed_tasks,
        "failed_tasks",          &T::failed_tasks,
        "cpu_usage_percent",     &T::cpu_usage_percent,
        "memory_usage_percent",  &T::memory_usage_percent,
        "available_memory_bytes",&T::available_memory_bytes,
        "pending_queue_size",    &T::pending_queue_size,
        "running_queue_size",    &T::running_queue_size,
        "completed_queue_size",  &T::completed_queue_size,
        "metadata",              &T::metadata
    >;
};
