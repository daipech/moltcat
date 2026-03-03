#include "state.hpp"
#include <glaze/glaze.hpp>
#include <chrono>
#include "../utils/error.hpp"

namespace moltcat::model {

auto StateManager::create_snapshot() -> StateSnapshot {
    StateSnapshot snapshot;

    // Set current timestamp
    const auto now = std::chrono::system_clock::now();
    const auto duration = now.time_since_epoch();
    snapshot.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    // Initialize state
    snapshot.system_state = SystemState::RUNNING;

    return snapshot;
}

auto StateManager::serialize_snapshot(const StateSnapshot& snapshot) -> std::string {
    std::string json_str;
    auto err = glz::write<glz::opts>(snapshot, json_str);

    if (err) {
        // TODO: Add appropriate error handling
        return "{}";
    }

    return json_str;
}

auto StateManager::deserialize_snapshot(std::string_view json_str) -> StateSnapshot {
    StateSnapshot snapshot;
    auto err = glz::read<glz::opts>(snapshot, json_str);

    if (err) {
        // TODO: Add appropriate error handling
        return StateSnapshot{};
    }

    return snapshot;
}

auto StateManager::update_system_state(StateSnapshot& snapshot, SystemState new_state) -> void {
    snapshot.system_state = new_state;
}

auto StateManager::increment_task_count(StateSnapshot& snapshot, TaskStatus status) -> void {
    // NOTE: Recommended to use TaskStateMachine to manage task state transitions
    // TaskStateMachine will automatically handle counter updates and retry logic
    //
    // Calling this function directly will only simply increment the counter for the corresponding status
    // Will not handle complex state transition logic (such as state rollback during retry)

    snapshot.total_tasks++;

    switch (status) {
        case TaskStatus::PENDING:
            snapshot.pending_tasks++;
            break;

        case TaskStatus::DISPATCHED:
            // DISPATCHED is an intermediate state, does not increment counter
            break;

        case TaskStatus::RUNNING:
            snapshot.running_tasks++;
            break;

        case TaskStatus::COMPLETED:
            snapshot.completed_tasks++;
            break;

        case TaskStatus::FAILED:
        case TaskStatus::TIMEOUT:
            snapshot.failed_tasks++;
            break;

        case TaskStatus::CANCELLED:
            // Cancelled tasks do not increment specific counters
            break;
    }
}

auto StateManager::update_agent_stats(StateSnapshot& snapshot,
                                      uint32_t total,
                                      uint32_t active,
                                      uint32_t idle,
                                      uint32_t error) -> void {
    snapshot.total_agents = total;
    snapshot.active_agents = active;
    snapshot.idle_agents = idle;
    snapshot.error_agents = error;
}

auto StateManager::update_resource_usage(StateSnapshot& snapshot,
                                         double cpu_percent,
                                         double memory_percent,
                                         uint64_t available_memory) -> void {
    snapshot.cpu_usage_percent = cpu_percent;
    snapshot.memory_usage_percent = memory_percent;
    snapshot.available_memory_bytes = available_memory;
}

} // namespace moltcat::model
