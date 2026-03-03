#pragma once

#include "types.hpp"
#include <glaze/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <chrono>

namespace moltcat::model {

/**
 * @brief Thread-safe task definition
 *
 * Synchronization strategy:
 * - High-frequency fields use atomic operations (lock-free high performance)
 * - STL containers use mutex protection
 * - Read-only fields require no protection
 */
struct Task {
    // ========== Read-only fields (no protection needed) ==========
    std::string id;                             // Task ID (UUID)
    std::string type;                           // Task type (e.g., "calculation", "query")
    TaskPriority priority{TaskPriority::NORMAL}; // Priority
    uint64_t created_at{0};                      // Creation timestamp (milliseconds)
    uint64_t timeout_ms{30000};                  // Timeout (milliseconds)
    glz::json_t payload;                        // Task payload data
    std::string description;                     // Task description
    std::string creator_id;                      // Creator ID
    std::string parent_task_id;                  // Parent task ID (for task decomposition)
    bool retry_on_failure{false};                // Retry on failure
    uint32_t max_retries{3};                     // Maximum retry count
    glz::json_t metadata;                        // Extended metadata

    // ========== High-frequency access fields (atomic operations) ==========
    // Cache line alignment, avoid false sharing
    alignas(64) std::atomic<TaskStatus> status{TaskStatus::PENDING};
    alignas(64) std::atomic<uint64_t> updated_at{0};
    alignas(64) std::atomic<uint64_t> started_at{0};
    alignas(64) std::atomic<uint64_t> completed_at{0};
    alignas(64) std::atomic<uint32_t> current_retry{0};

    // ========== Fields requiring lock protection ==========
    mutable std::mutex mutex_;                  // Protect following fields
    std::string agent_id_;                       // Assigned agent ID
    std::vector<std::string> dependencies_;      // Dependency task list
    std::map<std::string, std::string> tags_;    // Tags

    // ========== Thread-safe access methods ==========

    // ----- Status operations -----
    /**
     * @brief Set task status (thread-safe)
     */
    auto set_status(TaskStatus new_status) noexcept -> void {
        status.store(new_status, std::memory_order_release);
        updated_at.store(get_current_time_ms(), std::memory_order_release);
    }

    /**
     * @brief Get task status (thread-safe)
     */
    [[nodiscard]] auto get_status() const noexcept -> TaskStatus {
        return status.load(std::memory_order_acquire);
    }

    // ----- Agent ID operations -----
    /**
     * @brief Set agent ID (thread-safe)
     */
    auto set_agent_id(const std::string& id) -> void {
        std::lock_guard lock(mutex_);
        agent_id_ = id;
    }

    /**
     * @brief Get agent ID (thread-safe)
     */
    [[nodiscard]] auto get_agent_id() const -> std::string {
        std::lock_guard lock(mutex_);
        return agent_id_;
    }

    // ----- Dependency operations -----
    /**
     * @brief Add dependency task (thread-safe)
     */
    auto add_dependency(const std::string& dep_id) -> void {
        std::lock_guard lock(mutex_);
        dependencies_.push_back(dep_id);
    }

    /**
     * @brief Get dependency task list (thread-safe, returns copy)
     */
    [[nodiscard]] auto get_dependencies() const -> std::vector<std::string> {
        std::lock_guard lock(mutex_);
        return dependencies_;
    }

    /**
     * @brief Set dependency task list (thread-safe)
     */
    auto set_dependencies(const std::vector<std::string>& deps) -> void {
        std::lock_guard lock(mutex_);
        dependencies_ = deps;
    }

    // ----- Tag operations -----
    /**
     * @brief Set tag (thread-safe)
     */
    auto set_tag(const std::string& key, const std::string& value) -> void {
        std::lock_guard lock(mutex_);
        tags_[key] = value;
    }

    /**
     * @brief Get tag (thread-safe)
     */
    [[nodiscard]] auto get_tag(const std::string& key) const -> std::string {
        std::lock_guard lock(mutex_);
        auto it = tags_.find(key);
        return it != tags_.end() ? it->second : "";
    }

    /**
     * @brief Get all tags (thread-safe, returns copy)
     */
    [[nodiscard]] auto get_tags() const -> std::map<std::string, std::string> {
        std::lock_guard lock(mutex_);
        return tags_;
    }

    // ----- Retry operations -----
    /**
     * @brief Increment retry count (thread-safe)
     */
    auto increment_retry() noexcept -> uint32_t {
        return current_retry.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    /**
     * @brief Reset retry count (thread-safe)
     */
    auto reset_retry() noexcept -> void {
        current_retry.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Get retry count (thread-safe)
     */
    [[nodiscard]] auto get_retry_count() const noexcept -> uint32_t {
        return current_retry.load(std::memory_order_relaxed);
    }

    // ----- Timestamp operations -----
    /**
     * @brief Mark task started (thread-safe)
     */
    auto mark_started() noexcept -> void {
        auto now = get_current_time_ms();
        started_at.store(now, std::memory_order_release);
        updated_at.store(now, std::memory_order_release);
    }

    /**
     * @brief Mark task completed (thread-safe)
     */
    auto mark_completed() noexcept -> void {
        auto now = get_current_time_ms();
        completed_at.store(now, std::memory_order_release);
        updated_at.store(now, std::memory_order_release);
    }

    // ----- Serialization helper methods -----
    /**
     * @brief Get non-atomic status value for serialization
     */
    [[nodiscard]] auto get_status_value() const noexcept -> TaskStatus {
        return status.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get non-atomic timestamp value for serialization
     */
    [[nodiscard]] auto get_updated_at_value() const noexcept -> uint64_t {
        return updated_at.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto get_started_at_value() const noexcept -> uint64_t {
        return started_at.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto get_completed_at_value() const noexcept -> uint64_t {
        return completed_at.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto get_current_retry_value() const noexcept -> uint32_t {
        return current_retry.load(std::memory_order_relaxed);
    }

private:
    /**
     * @brief Get current timestamp (milliseconds)
     */
    [[nodiscard]] static auto get_current_time_ms() noexcept -> uint64_t {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
};

} // namespace moltcat::model

// Glaze serialization support (using non-atomic values)
template <>
struct glz::meta<moltcat::model::Task> {
    using T = moltcat::model::Task;

    // Custom serialization function
    static auto to_json(const Task& task) -> glz::json_t {
        return glz::json_t{
            {"id", task.id},
            {"type", task.type},
            {"priority", static_cast<int>(task.priority)},
            {"status", static_cast<int>(task.get_status_value())},
            {"created_at", task.created_at},
            {"updated_at", task.get_updated_at_value()},
            {"started_at", task.get_started_at_value()},
            {"completed_at", task.get_completed_at_value()},
            {"timeout_ms", task.timeout_ms},
            {"payload", task.payload},
            {"description", task.description},
            {"creator_id", task.creator_id},
            {"agent_id", task.get_agent_id()},
            {"parent_task_id", task.parent_task_id},
            {"dependencies", task.get_dependencies()},
            {"retry_on_failure", task.retry_on_failure},
            {"max_retries", task.max_retries},
            {"current_retry", task.get_current_retry_value()},
            {"metadata", task.metadata},
            {"tags", task.get_tags()}
        };
    }
};

// For backward compatibility, provide macro for direct field access (not recommended)
#define MOLT_TASK_LEGACY_ACCESS(task, field) task.field
