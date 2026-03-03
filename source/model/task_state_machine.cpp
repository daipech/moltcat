#include "task_state_machine.hpp"
#include <algorithm>
#include <mutex>

namespace moltcat::model {

// Static member initialization
std::vector<TaskStateMachine::TransitionCallback> TaskStateMachine::callbacks_;
std::mutex TaskStateMachine::callbacks_mutex_;

// ========== Public methods ==========

auto TaskStateMachine::transition(Task& task,
                                  StateSnapshot& snapshot,
                                  TaskStatus new_status) -> bool {
    auto old_status = task.get_status();

    // Check if state transition is valid
    if (!is_valid_transition(old_status, new_status)) {
        return false;
    }

    // Execute state transition
    task.set_status(new_status);

    // Update timestamp
    if (new_status == TaskStatus::RUNNING) {
        task.mark_started();
    } else if (new_status == TaskStatus::COMPLETED ||
               new_status == TaskStatus::FAILED ||
               new_status == TaskStatus::TIMEOUT ||
               new_status == TaskStatus::CANCELLED) {
        task.mark_completed();
    }

    // Update counters
    update_counters(snapshot, old_status, new_status);

    // Trigger callbacks
    notify_callbacks(task, old_status, new_status);

    return true;
}

auto TaskStateMachine::handle_completion(Task& task, StateSnapshot& snapshot) -> void {
    auto old_status = task.get_status();

    // Only RUNNING status can transition to COMPLETED
    if (old_status != TaskStatus::RUNNING) {
        return;
    }

    transition(task, snapshot, TaskStatus::COMPLETED);
}

auto TaskStateMachine::handle_failure(Task& task, StateSnapshot& snapshot) -> bool {
    auto old_status = task.get_status();

    // Only RUNNING or DISPATCHED status can fail
    if (old_status != TaskStatus::RUNNING && old_status != TaskStatus::DISPATCHED) {
        return false;
    }

    // Check if retry is possible
    if (can_retry(task)) {
        // Enter retry flow
        auto retry_count = task.increment_retry();

        // First set failure status (update counter but don't increment failed_tasks)
        task.set_status(TaskStatus::FAILED);
        task.mark_completed();

        // Remove from RUNNING
        snapshot.running_tasks--;

        // Return to PENDING status
        reset_to_pending(task, snapshot);

        return true;  // Enter retry
    } else {
        // Final failure
        transition(task, snapshot, TaskStatus::FAILED);
        return false;  // No retry
    }
}

auto TaskStateMachine::handle_timeout(Task& task, StateSnapshot& snapshot) -> bool {
    auto old_status = task.get_status();

    // Only RUNNING status can timeout
    if (old_status != TaskStatus::RUNNING) {
        return false;
    }

    // Check if retry is possible
    if (can_retry(task)) {
        // Enter retry flow
        task.increment_retry();

        // First set timeout status
        task.set_status(TaskStatus::TIMEOUT);
        task.mark_completed();

        // Remove from RUNNING
        snapshot.running_tasks--;

        // Return to PENDING status
        reset_to_pending(task, snapshot);

        return true;  // Enter retry
    } else {
        // Final timeout failure
        transition(task, snapshot, TaskStatus::TIMEOUT);
        return false;  // No retry
    }
}

auto TaskStateMachine::cancel(Task& task, StateSnapshot& snapshot) -> void {
    auto old_status = task.get_status();

    // Only PENDING, DISPATCHED, RUNNING status can be cancelled
    if (old_status != TaskStatus::PENDING &&
        old_status != TaskStatus::DISPATCHED &&
        old_status != TaskStatus::RUNNING) {
        return;  // Already in terminal state, cannot cancel
    }

    transition(task, snapshot, TaskStatus::CANCELLED);
}

auto TaskStateMachine::start(Task& task, StateSnapshot& snapshot) -> void {
    auto old_status = task.get_status();

    // Only PENDING or DISPATCHED status can start
    if (old_status != TaskStatus::PENDING && old_status != TaskStatus::DISPATCHED) {
        return;
    }

    transition(task, snapshot, TaskStatus::RUNNING);
}

auto TaskStateMachine::dispatch(Task& task, StateSnapshot& snapshot) -> void {
    auto old_status = task.get_status();

    // Only PENDING status can be dispatched
    if (old_status != TaskStatus::PENDING) {
        return;
    }

    transition(task, snapshot, TaskStatus::DISPATCHED);
}

[[nodiscard]] auto TaskStateMachine::is_valid_transition(TaskStatus old_status,
                                                         TaskStatus new_status) noexcept -> bool {
    // Define valid state transition rules
    switch (old_status) {
        case TaskStatus::PENDING:
            // PENDING can transition to: DISPATCHED, RUNNING, CANCELLED
            return new_status == TaskStatus::DISPATCHED ||
                   new_status == TaskStatus::RUNNING ||
                   new_status == TaskStatus::CANCELLED;

        case TaskStatus::DISPATCHED:
            // DISPATCHED can transition to: RUNNING, CANCELLED, PENDING (rollback)
            return new_status == TaskStatus::RUNNING ||
                   new_status == TaskStatus::CANCELLED ||
                   new_status == TaskStatus::PENDING;

        case TaskStatus::RUNNING:
            // RUNNING can transition to: COMPLETED, FAILED, TIMEOUT, CANCELLED
            return new_status == TaskStatus::COMPLETED ||
                   new_status == TaskStatus::FAILED ||
                   new_status == TaskStatus::TIMEOUT ||
                   new_status == TaskStatus::CANCELLED;

        case TaskStatus::COMPLETED:
        case TaskStatus::FAILED:
        case TaskStatus::TIMEOUT:
        case TaskStatus::CANCELLED:
            // Terminal states cannot transition to other states
            return false;

        default:
            return false;
    }
}

[[nodiscard]] auto TaskStateMachine::can_retry(const Task& task) noexcept -> bool {
    return task.retry_on_failure && !is_retry_exhausted(task);
}

[[nodiscard]] auto TaskStateMachine::is_retry_exhausted(const Task& task) noexcept -> bool {
    return task.get_retry_count() >= task.max_retries;
}

[[nodiscard]] auto TaskStateMachine::get_status_name(TaskStatus status) -> const char* {
    return to_string(status);
}

auto TaskStateMachine::register_callback(TransitionCallback callback) -> void {
    std::lock_guard lock(callbacks_mutex_);
    callbacks_.push_back(std::move(callback));
}

auto TaskStateMachine::clear_callbacks() -> void {
    std::lock_guard lock(callbacks_mutex_);
    callbacks_.clear();
}

// ========== Private methods ==========

auto TaskStateMachine::update_counters(StateSnapshot& snapshot,
                                       TaskStatus old_status,
                                       TaskStatus new_status) -> void {
    // Handle leaving old status
    switch (old_status) {
        case TaskStatus::PENDING:
            snapshot.pending_tasks--;
            break;

        case TaskStatus::DISPATCHED:
            // DISPATCHED is an intermediate state, does not affect counting
            break;

        case TaskStatus::RUNNING:
            snapshot.running_tasks--;
            break;

        case TaskStatus::COMPLETED:
        case TaskStatus::FAILED:
        case TaskStatus::TIMEOUT:
        case TaskStatus::CANCELLED:
            // Terminal states should not be left, no handling
            break;
    }

    // Handle entering new status
    switch (new_status) {
        case TaskStatus::PENDING:
            snapshot.pending_tasks++;
            break;

        case TaskStatus::DISPATCHED:
            // DISPATCHED is an intermediate state, does not affect counting
            break;

        case TaskStatus::RUNNING:
            snapshot.running_tasks++;
            break;

        case TaskStatus::COMPLETED:
            snapshot.completed_tasks++;
            break;

        case TaskStatus::FAILED:
        case TaskStatus::TIMEOUT:
            // Both failure and timeout count as failures
            snapshot.failed_tasks++;
            break;

        case TaskStatus::CANCELLED:
            // Cancelled tasks do not increment any counters
            break;
    }

    // Always increment total_tasks (only on first creation)
    if (old_status == TaskStatus::PENDING && new_status == TaskStatus::DISPATCHED) {
        snapshot.total_tasks++;
    }
}

auto TaskStateMachine::notify_callbacks(const Task& task,
                                        TaskStatus old_status,
                                        TaskStatus new_status) -> void {
    std::lock_guard lock(callbacks_mutex_);

    for (const auto& callback : callbacks_) {
        if (callback) {
            callback(task, old_status, new_status);
        }
    }
}

auto TaskStateMachine::reset_to_pending(Task& task, StateSnapshot& snapshot) -> void {
    // Reset to PENDING status
    task.set_status(TaskStatus::PENDING);

    // Increment pending counter
    snapshot.pending_tasks++;

    // NOTE: Do not increment total_tasks (because this is a retry)
    // NOTE: Do not decrement failed_tasks (because already handled in handle_failure/handle_timeout)
}

} // namespace moltcat::model
