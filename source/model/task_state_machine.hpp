#pragma once

#include "task.hpp"
#include "state.hpp"
#include <atomic>
#include <functional>

namespace moltcat::model {

/**
 * @brief Task state machine manager
 *
 * Responsible for unified management of task state transitions, retry logic, and counter updates
 * All state transitions must go through this class to ensure thread safety and consistency
 */
class TaskStateMachine {
public:
    /**
     * @brief State transition callback function type
     *
     * @param task Task object
     * @param old_status Old status
     * @param new_status New status
     */
    using TransitionCallback = std::function<void(const Task& task, TaskStatus old_status, TaskStatus new_status)>;

    /**
     * @brief Execute state transition
     *
     * @param task Task object
     * @param snapshot State snapshot (for updating counters)
     * @param new_status Target status
     * @return Whether transition succeeded
     */
    [[nodiscard]] static auto transition(Task& task,
                                         StateSnapshot& snapshot,
                                         TaskStatus new_status) -> bool;

    /**
     * @brief Handle task completion
     */
    static auto handle_completion(Task& task, StateSnapshot& snapshot) -> void;

    /**
     * @brief Handle task failure (automatically determines whether to retry)
     *
     * @param task Task object
     * @param snapshot State snapshot
     * @return Whether to enter retry flow
     */
    [[nodiscard]] static auto handle_failure(Task& task, StateSnapshot& snapshot) -> bool;

    /**
     * @brief Handle task timeout (automatically determines whether to retry)
     *
     * @param task Task object
     * @param snapshot State snapshot
     * @return Whether to enter retry flow
     */
    [[nodiscard]] static auto handle_timeout(Task& task, StateSnapshot& snapshot) -> bool;

    /**
     * @brief Cancel task
     */
    static auto cancel(Task& task, StateSnapshot& snapshot) -> void;

    /**
     * @brief Start task (PENDING -> RUNNING)
     */
    static auto start(Task& task, StateSnapshot& snapshot) -> void;

    /**
     * @brief Dispatch task (PENDING -> DISPATCHED)
     */
    static auto dispatch(Task& task, StateSnapshot& snapshot) -> void;

    /**
     * @brief Check if state transition is valid
     */
    [[nodiscard]] static auto is_valid_transition(TaskStatus old_status,
                                                   TaskStatus new_status) noexcept -> bool;

    /**
     * @brief Check if task can be retried
     */
    [[nodiscard]] static auto can_retry(const Task& task) noexcept -> bool;

    /**
     * @brief Check if task has reached maximum retry count
     */
    [[nodiscard]] static auto is_retry_exhausted(const Task& task) noexcept -> bool;

    /**
     * @brief Get status name (for logging)
     */
    [[nodiscard]] static auto get_status_name(TaskStatus status) -> const char*;

    /**
     * @brief Register state transition callback
     *
     * @param callback Callback function (thread-safe, called after state transition)
     */
    static auto register_callback(TransitionCallback callback) -> void;

    /**
     * @brief Clear all callbacks
     */
    static auto clear_callbacks() -> void;

private:
    /**
     * @brief Update status counters (internal method)
     */
    static auto update_counters(StateSnapshot& snapshot,
                                TaskStatus old_status,
                                TaskStatus new_status) -> void;

    /**
     * @brief Trigger state transition callbacks
     */
    static auto notify_callbacks(const Task& task,
                                 TaskStatus old_status,
                                 TaskStatus new_status) -> void;

    /**
     * @brief Reset task to PENDING status (for retry)
     */
    static auto reset_to_pending(Task& task, StateSnapshot& snapshot) -> void;

    // Global callback function list (thread-safe)
    static std::vector<TransitionCallback> callbacks_;
    static std::mutex callbacks_mutex_;
};

} // namespace moltcat::model
