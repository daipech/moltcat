#pragma once

#include "types.hpp"
#include <glaze/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <shared_mutex>

namespace moltcat::model {

/**
 * @brief Thread-safe execution context
 *
 * Synchronization strategy:
 * - Read-many-write-few fields use shared_mutex (read-write lock)
 * - High-frequency update fields use atomic operations
 * - Read-only fields require no protection
 */
struct Context {
    // ========== Read-only fields (no protection needed) ==========
    std::string context_id;                      // Context ID
    std::string task_id;                         // Associated task ID
    std::string agent_id;                        // Executing agent ID
    uint64_t created_at{0};                      // Creation timestamp
    glz::json_t metadata;                        // Extended metadata

    // ========== Read-many-write-few fields (using shared_mutex) ==========
    mutable std::shared_mutex rw_mutex_;         // Read-write lock protects following fields
    glz::json_t shared_state_;                   // State shared across agents
    glz::json_t session_data_;                   // Session-level data
    glz::json_t user_data_;                      // User-defined data
    std::map<std::string, std::string> user_metadata_; // User metadata
    std::vector<std::string> relevant_memories_;  // Relevant memory ID list
    std::string memory_context_;                 // Memory context (for retrieval)

    // ========== High-frequency update fields (atomic operations) ==========
    alignas(64) std::atomic<uint32_t> step_count{0};
    alignas(64) std::atomic<uint64_t> total_duration_ms{0};

    // ========== Configuration options (atomic operations) ==========
    std::atomic<bool> enable_logging{true};
    std::atomic<bool> enable_tracing{false};
    std::atomic<bool> enable_memory_storage{true};

    // ========== Thread-safe access methods ==========

    // ----- Shared state operations (read-write lock) -----
    /**
     * @brief Read shared state (allows multiple concurrent readers)
     */
    [[nodiscard]] auto get_shared_state() const -> glz::json_t {
        std::shared_lock lock(rw_mutex_);
        return shared_state_;
    }

    /**
     * @brief Update shared state (exclusive write)
     */
    auto set_shared_state(const glz::json_t& state) -> void {
        std::unique_lock lock(rw_mutex_);
        shared_state_ = state;
    }

    // ----- Session data operations (read-write lock) -----
    /**
     * @brief Read session data (allows multiple readers)
     */
    [[nodiscard]] auto get_session_data() const -> glz::json_t {
        std::shared_lock lock(rw_mutex_);
        return session_data_;
    }

    /**
     * @brief Update session data (exclusive write)
     */
    auto set_session_data(const glz::json_t& data) -> void {
        std::unique_lock lock(rw_mutex_);
        session_data_ = data;
    }

    // ----- User data operations (read-write lock) -----
    /**
     * @brief Read user data (allows multiple readers)
     */
    [[nodiscard]] auto get_user_data() const -> glz::json_t {
        std::shared_lock lock(rw_mutex_);
        return user_data_;
    }

    /**
     * @brief Update user data (exclusive write)
     */
    auto set_user_data(const glz::json_t& data) -> void {
        std::unique_lock lock(rw_mutex_);
        user_data_ = data;
    }

    // ----- User metadata operations (read-write lock) -----
    /**
     * @brief Set user metadata (exclusive write)
     */
    auto set_user_metadata(const std::string& key, const std::string& value) -> void {
        std::unique_lock lock(rw_mutex_);
        user_metadata_[key] = value;
    }

    /**
     * @brief Get user metadata (allows multiple readers)
     */
    [[nodiscard]] auto get_user_metadata(const std::string& key) const -> std::string {
        std::shared_lock lock(rw_mutex_);
        auto it = user_metadata_.find(key);
        return it != user_metadata_.end() ? it->second : "";
    }

    /**
     * @brief Get all user metadata (returns copy)
     */
    [[nodiscard]] auto get_all_user_metadata() const -> std::map<std::string, std::string> {
        std::shared_lock lock(rw_mutex_);
        return user_metadata_;
    }

    // ----- Memory-related operations (read-write lock) -----
    /**
     * @brief Add relevant memory (exclusive write)
     */
    auto add_relevant_memory(const std::string& memory_id) -> void {
        std::unique_lock lock(rw_mutex_);
        relevant_memories_.push_back(memory_id);
    }

    /**
     * @brief Get relevant memory list (allows multiple readers)
     */
    [[nodiscard]] auto get_relevant_memories() const -> std::vector<std::string> {
        std::shared_lock lock(rw_mutex_);
        return relevant_memories_;
    }

    /**
     * @brief Set memory context (exclusive write)
     */
    auto set_memory_context(const std::string& context) -> void {
        std::unique_lock lock(rw_mutex_);
        memory_context_ = context;
    }

    /**
     * @brief Get memory context (allows multiple readers)
     */
    [[nodiscard]] auto get_memory_context() const -> std::string {
        std::shared_lock lock(rw_mutex_);
        return memory_context_;
    }

    // ----- Execution statistics operations (atomic operations) -----
    /**
     * @brief Increment execution steps (atomic operation)
     */
    auto increment_step() noexcept -> uint32_t {
        return step_count.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    /**
     * @brief Get execution steps (atomic operation)
     */
    [[nodiscard]] auto get_step_count() const noexcept -> uint32_t {
        return step_count.load(std::memory_order_relaxed);
    }

    /**
     * @brief Update execution duration (atomic operation)
     */
    auto add_duration(uint64_t duration_ms) noexcept -> void {
        total_duration_ms.fetch_add(duration_ms, std::memory_order_relaxed);
    }

    /**
     * @brief Get execution duration (atomic operation)
     */
    [[nodiscard]] auto get_total_duration() const noexcept -> uint64_t {
        return total_duration_ms.load(std::memory_order_relaxed);
    }

    // ----- Configuration option operations (atomic operations) -----
    /**
     * @brief Enable/disable logging (atomic operation)
     */
    auto set_logging(bool enabled) noexcept -> void {
        enable_logging.store(enabled, std::memory_order_relaxed);
    }

    /**
     * @brief Check if logging is enabled (atomic operation)
     */
    [[nodiscard]] auto is_logging_enabled() const noexcept -> bool {
        return enable_logging.load(std::memory_order_relaxed);
    }

    /**
     * @brief Enable/disable tracing (atomic operation)
     */
    auto set_tracing(bool enabled) noexcept -> void {
        enable_tracing.store(enabled, std::memory_order_relaxed);
    }

    /**
     * @brief Check if tracing is enabled (atomic operation)
     */
    [[nodiscard]] auto is_tracing_enabled() const noexcept -> bool {
        return enable_tracing.load(std::memory_order_relaxed);
    }

    /**
     * @brief Enable/disable memory storage (atomic operation)
     */
    auto set_memory_storage(bool enabled) noexcept -> void {
        enable_memory_storage.store(enabled, std::memory_order_relaxed);
    }

    /**
     * @brief Check if memory storage is enabled (atomic operation)
     */
    [[nodiscard]] auto is_memory_storage_enabled() const noexcept -> bool {
        return enable_memory_storage.load(std::memory_order_relaxed);
    }

    // ----- Serialization helper methods -----
    /**
     * @brief Get non-atomic value for serialization
     */
    [[nodiscard]] auto get_step_count_value() const noexcept -> uint32_t {
        return step_count.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto get_total_duration_value() const noexcept -> uint64_t {
        return total_duration_ms.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto get_enable_logging_value() const noexcept -> bool {
        return enable_logging.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto get_enable_tracing_value() const noexcept -> bool {
        return enable_tracing.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto get_enable_memory_storage_value() const noexcept -> bool {
        return enable_memory_storage.load(std::memory_order_relaxed);
    }
};

} // namespace moltcat::model

// Glaze serialization support (using non-atomic values)
template <>
struct glz::meta<moltcat::model::Context> {
    using T = moltcat::model::Context;

    // Custom serialization function
    static auto to_json(const Context& ctx) -> glz::json_t {
        return glz::json_t{
            {"context_id", ctx.context_id},
            {"task_id", ctx.task_id},
            {"agent_id", ctx.agent_id},
            {"created_at", ctx.created_at},
            {"shared_state", ctx.get_shared_state()},
            {"session_data", ctx.get_session_data()},
            {"user_data", ctx.get_user_data()},
            {"user_metadata", ctx.get_all_user_metadata()},
            {"relevant_memories", ctx.get_relevant_memories()},
            {"memory_context", ctx.get_memory_context()},
            {"step_count", ctx.get_step_count_value()},
            {"total_duration_ms", ctx.get_total_duration_value()},
            {"enable_logging", ctx.get_enable_logging_value()},
            {"enable_tracing", ctx.get_enable_tracing_value()},
            {"enable_memory_storage", ctx.get_enable_memory_storage_value()},
            {"metadata", ctx.metadata}
        };
    }
};
