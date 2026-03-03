#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <chrono>
#include <functional>
#include <atomic>
#include <queue>
#include <condition_variable>

#include "connection_info.hpp"
#include "../utils/error.hpp"
#include "../messaging/message_bus_client.hpp"

namespace moltcat::gateway {

/**
 * @brief Task status
 */
enum class TaskStatus {
    PENDING = 0,       // Waiting to be executed
    RUNNING = 1,       // Currently executing
    COMPLETED = 2,     // Successfully completed
    FAILED = 3,        // Failed
    CANCELLED = 4       // Cancelled
};

/**
 * @brief Task priority
 */
enum class TaskPriority : uint32_t {
    LOW = 3,           // Low priority
    NORMAL = 5,        // Normal priority
    HIGH = 7,          // High priority
    URGENT = 9         // Urgent priority
};

/**
 * @brief Task information
 */
struct Task {
    std::string task_id;              // Task ID
    std::string type;                // Task type
    glz::json_t params;              // Task parameters

    TaskStatus status = TaskStatus::PENDING;
    TaskPriority priority = TaskPriority::NORMAL;

    // Creation information
    std::string creator_device_id;     // Creator device ID
    std::string creator_connection_id; // Creator connection ID
    std::chrono::system_clock::time_point created_at;

    // Execution information
    std::string assigned_agent_id;     // Assigned Agent ID
    std::chrono::system_clock::time_point started_at;

    // Completion information
    glz::json_t result;               // Execution result
    std::string error_message;        // Error message
    std::chrono::system_clock::time_point completed_at;

    // Timeout configuration
    std::chrono::seconds timeout{300};  // Timeout duration (default 5 minutes)
    std::chrono::system_clock::time_point deadline;  // Deadline

    // Idempotency
    std::string idempotency_key;      // Idempotency key

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] auto to_json() const -> glz::json_t;

    /**
     * @brief Get status string
     */
    [[nodiscard]] auto status_string() const -> std::string;

    /**
     * @brief Check if timed out
     */
    [[nodiscard]] auto is_timeout() const -> bool;
};

/**
 * @brief Task manager
 *
 * Responsible for task creation, scheduling, and status tracking
 */
class TaskManager {
public:
    /**
     * @brief Constructor
     */
    TaskManager();

    /**
     * @brief Destructor
     */
    ~TaskManager() = default;

    // Disable copy
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;

    /**
     * @brief Set message bus client
     *
     * @param message_bus_client Message bus client (pointer, non-owning)
     */
    auto set_message_bus_client(messaging::MessageBusClient* message_bus_client)
        -> void;

    /**
     * @brief Create task
     *
     * @param type Task type
     * @param params Task parameters
     * @param creator_id Creator device ID
     * @param connection_id Creator connection ID
     * @param idempotency_key Idempotency key (optional)
     * @param priority Task priority (optional)
     * @param timeout_seconds Timeout duration (optional, default 300 seconds)
     * @return Result<std::string> Task ID, or error
     */
    [[nodiscard]] auto create_task(
        std::string_view type,
        const glz::json_t& params,
        std::string_view creator_id,
        std::string_view connection_id,
        std::string_view idempotency_key = "",
        TaskPriority priority = TaskPriority::NORMAL,
        uint64_t timeout_seconds = 300
    ) -> Result<std::string>;

    /**
     * @brief Get task
     *
     * @param task_id Task ID
     * @return std::optional<Task> Task information, returns nullopt if not exists
     */
    [[nodiscard]] auto get_task(std::string_view task_id) const
        -> std::optional<Task>;

    /**
     * @brief List tasks
     *
     * @param status Status filter (optional)
     * @param limit Limit count (optional)
     * @param offset Offset (optional)
     * @return std::vector<Task> Task list
     */
    [[nodiscard]] auto list_tasks(
        std::optional<TaskStatus> status = std::nullopt,
        std::optional<size_t> limit = std::nullopt,
        std::optional<size_t> offset = std::nullopt
    ) const -> std::vector<Task>;

    /**
     * @brief Cancel task
     *
     * @param task_id Task ID
     * @param requester_id Requester device ID
     * @param reason Cancellation reason (optional)
     * @return Result<void>
     */
    auto cancel_task(
        std::string_view task_id,
        std::string_view requester_id,
        std::string_view reason = ""
    ) -> Result<void>;

    /**
     * @brief Update task status
     *
     * @param task_id Task ID
     * @param status New status
     * @param result Execution result (optional)
     * @param error Error message (optional)
     * @return Result<void>
     */
    auto update_task_status(
        std::string_view task_id,
        TaskStatus status,
        const glz::json_t* result = nullptr,
        std::string_view error = ""
    ) -> Result<void>;

    /**
     * @brief Assign Agent to task
     *
     * @param task_id Task ID
     * @param agent_id Agent ID
     * @return Result<void>
     */
    auto assign_agent(std::string_view task_id, std::string_view agent_id)
        -> Result<void>;

    /**
     * @brief Get task statistics
     *
     * @return glz::json_t Statistics information
     */
    [[nodiscard]] auto get_statistics() const -> glz::json_t;

    /**
     * @brief Register task type handler
     *
     * @param type Task type
     * @param handler Handler function
     */
    auto register_handler(
        std::string_view type,
        std::function<Result<glz::json_t>(const Task&)> handler
    ) -> void;

    /**
     * @brief Execute task
     *
     * @param task_id Task ID
     * @return Result<void>
     */
    auto execute_task(std::string_view task_id) -> Result<void>;

    /**
     * @brief Handle task message (received from MessageBus)
     *
     * @param message Message
     */
    auto handle_message(const messaging::Message& message) -> void;

    /**
     * @brief Get next pending task (priority queue)
     *
     * @return std::optional<std::string> Task ID, returns nullopt if no tasks
     */
    [[nodiscard]] auto get_next_pending_task() -> std::optional<std::string>;

    /**
     * @brief Check and handle timeout tasks
     *
     * @return size_t Number of cancelled timeout tasks
     */
    auto check_timeouts() -> size_t;

    /**
     * @brief Start timeout checker thread
     */
    auto start_timeout_checker() -> void;

    /**
     * @brief Stop timeout checker thread
     */
    auto stop_timeout_checker() -> void;

private:
    // Task storage
    std::unordered_map<std::string, Task> tasks_;
    mutable std::shared_mutex tasks_mutex_;

    // Task type handlers
    std::unordered_map<std::string, std::function<Result<glz::json_t>(const Task&)>> handlers_;

    // Message bus client (non-owning, externally managed lifecycle)
    messaging::MessageBusClient* message_bus_client_{nullptr};

    // Statistics
    std::atomic<size_t> total_created_{0};
    std::atomic<size_t> total_completed_{0};
    std::atomic<size_t> total_failed_{0};
    std::atomic<size_t> total_cancelled_{0};
    std::atomic<size_t> total_timeout_{0};

    // Priority queue (task ID + priority comparison)
    struct PriorityTask {
        std::string task_id;
        TaskPriority priority;
        uint64_t created_timestamp;

        // Priority queue comparison function: higher priority first, earlier creation time first for same priority
        bool operator<(const PriorityTask& other) const {
            if (priority != other.priority) {
                return static_cast<uint32_t>(priority) < static_cast<uint32_t>(other.priority);
            }
            return created_timestamp > other.created_timestamp;
        }
    };
    std::priority_queue<PriorityTask> pending_queue_;
    std::mutex queue_mutex_;

    // Timeout checking
    std::thread timeout_thread_;
    std::atomic<bool> timeout_checker_running_{false};
    std::condition_variable timeout_cv_;
    std::mutex timeout_mutex_;
    static constexpr size_t TIMEOUT_CHECK_INTERVAL_SECONDS = 30;  // Check every 30 seconds

    /**
     * @brief Generate task ID
     */
    [[nodiscard]] auto generate_task_id() -> std::string;

    /**
     * @brief Check if task exists
     */
    [[nodiscard]] auto task_exists(std::string_view task_id) const -> bool;

    /**
     * @brief Send task event
     */
    auto send_task_event(
        std::string_view event_type,
        const Task& task,
        const std::optional<std::string>& target_connection = std::nullopt
    ) -> void;

    /**
     * @brief Send task event to MessageBus
     */
    auto send_task_event_to_bus(
        messaging::MessageType msg_type,
        const Task& task
    ) -> void;

    /**
     * @brief Remove task from priority queue
     */
    auto remove_from_queue(const std::string& task_id) -> void;

    /**
     * @brief Timeout checker thread main function
     */
    auto timeout_checker_loop() -> void;
};

} // namespace moltcat::gateway
