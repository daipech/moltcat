#include "task_manager.hpp"
#include "../utils/string_utils.hpp"
#include "../utils/uuid.hpp"
#include <algorithm>

namespace moltcat::gateway {

// ==================== Task Implementation ====================

auto Task::to_json() const -> glz::json_t {
    glz::json_t json;
    json["taskId"] = task_id;
    json["type"] = type;
    json["params"] = params;
    json["status"] = status_string();
    json["priority"] = static_cast<uint32_t>(priority);

    auto to_ms = [](const auto& tp) {
        if (tp == std::chrono::system_clock::time_point{}) {
            return 0L;
        }
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()
        ).count();
    };

    json["createdAt"] = to_ms(created_at);

    if (status == TaskStatus::RUNNING || status == TaskStatus::COMPLETED ||
        status == TaskStatus::FAILED || status == TaskStatus::CANCELLED) {
        json["startedAt"] = to_ms(started_at);
    }

    if (status == TaskStatus::COMPLETED || status == TaskStatus::FAILED ||
        status == TaskStatus::CANCELLED) {
        json["completedAt"] = to_ms(completed_at);
    }

    if (!result.is_null() && result.size() > 0) {
        json["result"] = result;
    }

    if (!error_message.empty()) {
        json["error"] = error_message;
    }

    if (!assigned_agent_id.empty()) {
        json["agentId"] = assigned_agent_id;
    }

    // Timeout information
    json["timeout"] = timeout.count();
    if (deadline != std::chrono::system_clock::time_point{}) {
        json["deadline"] = to_ms(deadline);
    }

    return json;
}

auto Task::status_string() const -> std::string {
    switch (status) {
        case TaskStatus::PENDING: return "pending";
        case TaskStatus::RUNNING: return "running";
        case TaskStatus::COMPLETED: return "completed";
        case TaskStatus::FAILED: return "failed";
        case TaskStatus::CANCELLED: return "cancelled";
    }
    return "unknown";
}

auto Task::is_timeout() const -> bool {
    if (deadline == std::chrono::system_clock::time_point{}) {
        return false;  // No deadline set
    }
    return std::chrono::system_clock::now() > deadline;
}

// ==================== TaskManager Implementation ====================

TaskManager::TaskManager() {
    // Register default task handlers (examples)
    register_handler("code_generation", [](const Task& task) -> Result<glz::json_t> {
        // Simulate code generation
        glz::json_t result;
        result["output"] = "#include <algorithm>\n// Quicksort implementation";
        result["language"] = "cpp";
        result["lines"] = 42;
        return Ok(result);
    });

    register_handler("code_review", [](const Task& task) -> Result<glz::json_t> {
        // Simulate code review
        glz::json_t result;
        result["issues"] = glz::json_t::array();
        result["summary"] = "Code quality is good";
        return Ok(result);
    });

    // Start timeout checker thread
    start_timeout_checker();
}

auto TaskManager::set_message_bus_client(messaging::MessageBusClient* message_bus_client)
    -> void {
    message_bus_client_ = message_bus_client;
}

auto TaskManager::create_task(
    std::string_view type,
    const glz::json_t& params,
    std::string_view creator_id,
    std::string_view connection_id,
    std::string_view idempotency_key,
    TaskPriority priority,
    uint64_t timeout_seconds
) -> Result<std::string> {
    // Check idempotency key
    if (!idempotency_key.empty()) {
        std::shared_lock lock(tasks_mutex_);
        for (const auto& [id, task] : tasks_) {
            if (task.idempotency_key == idempotency_key) {
                return Ok(std::string(id));  // Return existing task ID
            }
        }
    }

    // Create task
    Task task;
    task.task_id = generate_task_id();
    task.type = type;
    task.params = params;
    task.status = TaskStatus::PENDING;
    task.priority = priority;
    task.creator_device_id = creator_id;
    task.creator_connection_id = connection_id;
    task.created_at = std::chrono::system_clock::now();
    task.timeout = std::chrono::seconds(timeout_seconds);
    task.deadline = task.created_at + task.timeout;

    if (!idempotency_key.empty()) {
        task.idempotency_key = idempotency_key;
    }

    // Store task
    {
        std::unique_lock lock(tasks_mutex_);
        tasks_[task.task_id] = task;
    }

    // Add to priority queue
    {
        std::lock_guard queue_lock(queue_mutex_);
        auto created_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            task.created_at.time_since_epoch()
        ).count();
        pending_queue_.push({task.task_id, priority, static_cast<uint64_t>(created_ms)});
    }

    total_created_++;

    // Send task creation event
    send_task_event("task.created", task, connection_id);
    send_task_event_to_bus(messaging::MessageType::TASK_SUBMIT, task);

    // TODO: Send to MessageBus for AgentTeam scheduling

    return Ok(task.task_id);
}

auto TaskManager::get_task(std::string_view task_id) const
    -> std::optional<Task> {
    std::shared_lock lock(tasks_mutex_);

    auto it = tasks_.find(std::string(task_id));
    if (it != tasks_.end()) {
        return it->second;
    }

    return std::nullopt;
}

auto TaskManager::list_tasks(
    std::optional<TaskStatus> status,
    std::optional<size_t> limit,
    std::optional<size_t> offset
) const -> std::vector<Task> {
    std::shared_lock lock(tasks_mutex_);

    std::vector<Task> result;

    for (const auto& [id, task] : tasks_) {
        // Status filtering
        if (status.has_value() && task.status != status.value()) {
            continue;
        }

        result.push_back(task);
    }

    // Sort by creation time (newest first)
    std::sort(result.begin(), result.end(),
        [](const Task& a, const Task& b) {
            return a.created_at > b.created_at;
        });

    // Pagination
    size_t start = offset.value_or(0);
    size_t count = limit.value_or(result.size());

    if (start < result.size()) {
        size_t end = std::min(start + count, result.size());
        return std::vector<Task>(result.begin() + start, result.begin() + end);
    }

    return {};
}

auto TaskManager::cancel_task(
    std::string_view task_id,
    std::string_view requester_id,
    std::string_view reason
) -> Result<void> {
    std::unique_lock lock(tasks_mutex_);

    auto it = tasks_.find(std::string(task_id));
    if (it == tasks_.end()) {
        return Err<void>(
            ErrorCode::NOT_FOUND,
            std::format("Task not found: {}", task_id)
        );
    }

    auto& task = it->second;

    // Check task status
    if (task.status == TaskStatus::COMPLETED ||
        task.status == TaskStatus::CANCELLED) {
        return Err<void>(
            ErrorCode::INVALID_ARGUMENT,
            std::format("Task is {}, cannot cancel", task.status_string())
        );
    }

    // Update status
    task.status = TaskStatus::CANCELLED;
    task.completed_at = std::chrono::system_clock::now();
    task.error_message = reason;

    total_cancelled_++;

    lock.unlock();

    // Send task cancellation event
    send_task_event("task.cancelled", task, task.creator_connection_id);
    send_task_event_to_bus(messaging::MessageType::TASK_CANCEL, task);

    // TODO: Notify AgentTeam to cancel execution

    return Ok();
}

auto TaskManager::update_task_status(
    std::string_view task_id,
    TaskStatus status,
    const glz::json_t* result,
    std::string_view error
) -> Result<void> {
    std::unique_lock lock(tasks_mutex_);

    auto it = tasks_.find(std::string(task_id));
    if (it == tasks_.end()) {
        return Err<void>(
            ErrorCode::NOT_FOUND,
            std::format("Task not found: {}", task_id)
        );
    }

    auto& task = it->second;

    // Update status
    task.status = status;

    if (status == TaskStatus::RUNNING) {
        task.started_at = std::chrono::system_clock::now();
    } else if (status == TaskStatus::COMPLETED) {
        task.completed_at = std::chrono::system_clock::now();
        if (result) {
            task.result = *result;
        }
        total_completed_++;
    } else if (status == TaskStatus::FAILED) {
        task.completed_at = std::chrono::system_clock::now();
        task.error_message = error;
        total_failed_++;
    }

    lock.unlock();

    // Send task status event
    std::string event_type;
    messaging::MessageType msg_type;
    switch (status) {
        case TaskStatus::RUNNING:
            event_type = "task.started";
            msg_type = messaging::MessageType::TASK_EXECUTE;
            break;
        case TaskStatus::COMPLETED:
            event_type = "task.completed";
            msg_type = messaging::MessageType::TASK_RESULT;
            break;
        case TaskStatus::FAILED:
            event_type = "task.failed";
            msg_type = messaging::MessageType::TASK_RESULT;
            break;
        default:
            return Ok();
    }

    send_task_event(event_type, task, task.creator_connection_id);
    send_task_event_to_bus(msg_type, task);

    return Ok();
}

auto TaskManager::assign_agent(std::string_view task_id, std::string_view agent_id)
    -> Result<void> {
    std::unique_lock lock(tasks_mutex_);

    auto it = tasks_.find(std::string(task_id));
    if (it == tasks_.end()) {
        return Err<void>(
            ErrorCode::NOT_FOUND,
            std::format("Task not found: {}", task_id)
        );
    }

    auto& task = it->second;
    task.assigned_agent_id = agent_id;

    return Ok();
}

auto TaskManager::get_statistics() const -> glz::json_t {
    std::shared_lock lock(tasks_mutex_);

    glz::json_t stats;
    stats["total"] = tasks_.size();
    stats["pending"] = 0;
    stats["running"] = 0;
    stats["completed"] = total_completed_.load();
    stats["failed"] = total_failed_.load();
    stats["cancelled"] = total_cancelled_.load();
    stats["timeout"] = total_timeout_.load();

    for (const auto& [id, task] : tasks_) {
        switch (task.status) {
            case TaskStatus::PENDING: stats["pending"] = stats["pending"].get<int>() + 1; break;
            case TaskStatus::RUNNING: stats["running"] = stats["running"].get<int>() + 1; break;
            default: break;
        }
    }

    return stats;
}

auto TaskManager::register_handler(
    std::string_view type,
    std::function<Result<glz::json_t>(const Task&)> handler
) -> void {
    handlers_[std::string(type)] = std::move(handler);
}

auto TaskManager::execute_task(std::string_view task_id) -> Result<void> {
    // Get task
    auto task_opt = get_task(task_id);
    if (!task_opt.has_value()) {
        return Err<void>(
            ErrorCode::NOT_FOUND,
            std::format("Task not found: {}", task_id)
        );
    }

    auto& task = task_opt.value();

    // Find handler
    auto handler_it = handlers_.find(task.type);
    if (handler_it == handlers_.end()) {
        update_task_status(task_id, TaskStatus::FAILED, nullptr,
            "Unsupported task type");
        return Err<void>(
            ErrorCode::NOT_IMPLEMENTED,
            std::format("Unsupported task type: {}", task.type)
        );
    }

    // Update status to running
    update_task_status(task_id, TaskStatus::RUNNING);

    // Execute handler
    auto result = handler_it->second(task);

    if (!result.has_value()) {
        update_task_status(task_id, TaskStatus::FAILED, nullptr,
            result.error().message());
        return Err<void>(result.error().code(), result.error().message());
    }

    // Update status to completed
    update_task_status(task_id, TaskStatus::COMPLETED, &result.value());

    return Ok();
}

auto TaskManager::handle_message(const messaging::Message& message) -> void {
    // TODO: Handle messages from MessageBus
    // Examples: Agent status updates, task progress, etc.
}

auto TaskManager::generate_task_id() -> std::string {
    return "task_" + utils::UUID::generate();
}

auto TaskManager::task_exists(std::string_view task_id) const -> bool {
    std::shared_lock lock(tasks_mutex_);
    return tasks_.contains(std::string(task_id));
}

auto TaskManager::send_task_event(
    std::string_view event_type,
    const Task& task,
    const std::optional<std::string>& target_connection
) -> void {
    // TODO: Send task event to Gateway
    // If target_connection has value, only send to specified connection
    // Otherwise broadcast to all related connections

    // Currently placeholder: actually need to access WsGateway/HttpGateway broadcast functionality
    std::cout << "[TaskManager] Event: " << event_type
              << ", Task: " << task.task_id << std::endl;
}

auto TaskManager::send_task_event_to_bus(
    messaging::MessageType msg_type,
    const Task& task
) -> void {
    if (!message_bus_client_) {
        return;
    }

    try {
        // Create message
        auto msg = messaging::Message::create(msg_type, "TaskManager");
        msg.header.destination = "AgentTeam";  // Send to AgentTeam service
        msg.header.priority = static_cast<uint32_t>(task.priority);

        // Build message body
        glz::json_t body;
        body["task_id"] = task.task_id;
        body["type"] = task.type;
        body["status"] = task.status_string();
        body["priority"] = static_cast<uint32_t>(task.priority);
        body["creator"] = task.creator_device_id;
        body["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            task.created_at.time_since_epoch()
        ).count();

        if (task.status == TaskStatus::RUNNING ||
            task.status == TaskStatus::COMPLETED ||
            task.status == TaskStatus::FAILED) {
            body["agent_id"] = task.assigned_agent_id;
        }

        if (task.status == TaskStatus::COMPLETED && !task.result.is_null()) {
            body["result"] = task.result;
        }

        if (task.status == TaskStatus::FAILED) {
            body["error"] = task.error_message;
        }

        std::string body_str;
        glz::write_json(body, body_str);
        msg.body.content = body_str;

        // Send to MessageBus (broadcast to all AgentTeam instances)
        size_t sent_count = message_bus_client_->broadcast("AgentTeam", msg);

        std::cout << "[TaskManager] Event sent to MessageBus: "
                  << static_cast<uint32_t>(msg_type)
                  << ", Task: " << task.task_id
                  << ", Recipient count: " << sent_count << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[TaskManager] Failed to send event to MessageBus: "
                  << e.what() << std::endl;
    }
}

auto TaskManager::get_next_pending_task() -> std::optional<std::string> {
    std::lock_guard lock(queue_mutex_);

    while (!pending_queue_.empty()) {
        auto top = pending_queue_.top();
        pending_queue_.pop();

        // Check if task is still in queue (may have been cancelled)
        std::shared_lock task_lock(tasks_mutex_);
        auto it = tasks_.find(top.task_id);
        if (it != tasks_.end() && it->second.status == TaskStatus::PENDING) {
            return top.task_id;
        }
    }

    return std::nullopt;
}

auto TaskManager::remove_from_queue(const std::string& task_id) -> void {
    // Priority queue does not support direct deletion, use mark-sweep strategy
    // Actual deletion performed in get_next_pending_task
}

auto TaskManager::start_timeout_checker() -> void {
    if (timeout_checker_running_.exchange(true)) {
        return;  // Already running
    }

    timeout_thread_ = std::thread([this]() {
        timeout_checker_loop();
    });

    std::cout << "[TaskManager] Timeout checker thread started" << std::endl;
}

auto TaskManager::stop_timeout_checker() -> void {
    timeout_checker_running_.store(false);

    {
        std::lock_guard lock(timeout_mutex_);
        timeout_cv_.notify_all();
    }

    if (timeout_thread_.joinable()) {
        timeout_thread_.join();
    }

    std::cout << "[TaskManager] Timeout checker thread stopped" << std::endl;
}

auto TaskManager::timeout_checker_loop() -> void {
    while (timeout_checker_running_.load()) {
        // Wait for specified time or stop signal
        std::unique_lock lock(timeout_mutex_);
        timeout_cv_.wait_for(lock, std::chrono::seconds(TIMEOUT_CHECK_INTERVAL_SECONDS));

        if (!timeout_checker_running_.load()) {
            break;
        }

        // Check for timeouts
        auto timeout_count = check_timeouts();
        if (timeout_count > 0) {
            std::cout << "[TaskManager] Cancelled " << timeout_count
                      << " timeout tasks" << std::endl;
        }
    }
}

auto TaskManager::check_timeouts() -> size_t {
    size_t timeout_count = 0;
    std::vector<std::string> timeout_tasks;

    // Find all timeout tasks
    {
        std::shared_lock lock(tasks_mutex_);
        for (const auto& [id, task] : tasks_) {
            if ((task.status == TaskStatus::PENDING || task.status == TaskStatus::RUNNING)
                && task.is_timeout()) {
                timeout_tasks.push_back(id);
            }
        }
    }

    // Cancel timeout tasks
    for (const auto& task_id : timeout_tasks) {
        auto result = cancel_task(task_id, "system", "Task timeout");
        if (result.has_value()) {
            timeout_count++;
            total_timeout_++;
        }
    }

    return timeout_count;
}

} // namespace moltcat::gateway
