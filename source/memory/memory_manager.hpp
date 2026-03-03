#pragma once

#include "memory_entry.hpp"
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <functional>

namespace moltcat::memory {

/**
 * @brief Memory manager
 *
 * Global singleton, responsible for memory storage, retrieval, sharing, and access control
 *
 * Core features:
 * - Memory CRUD operations
 * - Multi-level indexing (owner, tag, task)
 * - Access control and permission verification
 * - Semantic retrieval (reserved interface)
 * - Memory sharing mechanism
 */
class MemoryManager {
public:
    /**
     * @brief Get global singleton
     */
    [[nodiscard]] static auto get_instance() -> MemoryManager&;

    // Disable copy and move
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;
    MemoryManager(MemoryManager&&) = delete;
    MemoryManager& operator=(MemoryManager&&) = delete;

    /**
     * @brief Destructor
     */
    ~MemoryManager() = default;

    // ========== Memory Write ==========

    /**
     * @brief Store memory
     *
     * @param memory Memory entry
     * @return Memory ID (auto-generated if memory_id is empty)
     */
    [[nodiscard]] auto store_memory(const MemoryEntry& memory) -> std::string;

    /**
     * @brief Batch store memories
     *
     * @param memories Memory entry list
     * @return Memory ID list
     */
    auto store_memories(const std::vector<MemoryEntry>& memories)
        -> std::vector<std::string>;

    // ========== Memory Read ==========

    /**
     * @brief Retrieve memory (with permission check)
     *
     * @param memory_id Memory ID
     * @param agent_id Agent ID requesting access
     * @return Memory entry (if exists and has permission)
     */
    [[nodiscard]] auto retrieve_memory(
        std::string_view memory_id,
        std::string_view agent_id
    ) const -> std::optional<MemoryEntry>;

    /**
     * @brief Batch retrieve memories
     *
     * @param memory_ids Memory ID list
     * @param agent_id Agent ID requesting access
     * @return Memory entry list (only return those with permission)
     */
    [[nodiscard]] auto retrieve_memories(
        const std::vector<std::string>& memory_ids,
        std::string_view agent_id
    ) const -> std::vector<MemoryEntry>;

    // ========== Memory Search ==========

    /**
     * @brief Semantic search (based on vector similarity)
     *
     * @param query_embedding Query vector
     * @param agent_id Agent ID requesting search
     * @param top_k Return top K results
     * @param min_similarity Minimum similarity threshold [0.0, 1.0]
     * @return Matching memory list (descending by similarity)
     *
     * Note: Phase 2 only implements basic framework, actual embedding generation and vector retrieval will be implemented in later phases
     */
    [[nodiscard]] auto semantic_search(
        std::string_view query_embedding,
        std::string_view agent_id,
        size_t top_k = 10,
        float min_similarity = 0.7f
    ) const -> std::vector<MemoryEntry>;

    /**
     * @brief Tag search
     *
     * @param tags Tag list (AND relationship: all tags must match)
     * @param agent_id Agent ID requesting search
     * @return Matching memory list
     */
    [[nodiscard]] auto search_by_tags(
        const std::vector<std::string>& tags,
        std::string_view agent_id
    ) const -> std::vector<MemoryEntry>;

    /**
     * @brief Task-related memories
     *
     * @param task_id Task ID
     * @param agent_id Agent ID requesting access
     * @return All memories related to this task
     */
    [[nodiscard]] auto get_task_memories(
        std::string_view task_id,
        std::string_view agent_id
    ) const -> std::vector<MemoryEntry>;

    /**
     * @brief Get all memories visible to Agent
     *
     * @param agent_id Agent ID
     * @return All memories this Agent has permission to access
     */
    [[nodiscard]] auto get_visible_memories(std::string_view agent_id) const
        -> std::vector<MemoryEntry>;

    // ========== Memory Sharing ==========

    /**
     * @brief Share memory with other Agent
     *
     * Change memory visibility from PRIVATE to SHARED, and add target Agent to allowed_readers
     *
     * @param memory_id Memory ID
     * @param owner_agent_id Memory owner ID
     * @param target_agent_id Target Agent ID
     * @return Whether successful
     */
    auto share_memory(
        std::string_view memory_id,
        std::string_view owner_agent_id,
        std::string_view target_agent_id
    ) -> bool;

    /**
     * @brief Batch share memories
     *
     * @param memory_id Memory ID
     * @param owner_agent_id Memory owner ID
     * @param target_agent_ids Target Agent ID list
     * @return Number of Agents successfully shared with
     */
    auto share_memory_with_multiple(
        std::string_view memory_id,
        std::string_view owner_agent_id,
        const std::vector<std::string>& target_agent_ids
    ) -> size_t;

    /**
     * @brief Create shared context
     *
     * Create temporary shared context for a group of Agents, for collaborative tasks
     *
     * @param participant_ids Participant Agent ID list
     * @param context_name Context name
     * @return Context ID
     */
    [[nodiscard]] auto create_shared_context(
        const std::vector<std::string>& participant_ids,
        std::string_view context_name = "shared_context"
    ) -> std::string;

    // ========== Memory Management ==========

    /**
     * @brief Update access statistics
     *
     * @param memory_id Memory ID
     * @return Whether successful
     */
    auto update_access_stats(std::string_view memory_id) -> bool;

    /**
     * @brief Delete memory
     *
     * @param memory_id Memory ID
     * @param requesting_agent_id Agent ID requesting deletion (must be owner)
     * @return Whether successful
     */
    auto delete_memory(
        std::string_view memory_id,
        std::string_view requesting_agent_id
    ) -> bool;

    /**
     * @brief Archive old memories
     *
     * Archive low-importance memories before specified time
     *
     * @param before_timestamp Time threshold (milliseconds)
     * @param agent_id Agent ID (only archive this Agent's memories)
     * @param max_keep Maximum number of memories to keep
     * @return Number of archived memories
     */
    auto archive_old_memories(
        uint64_t before_timestamp,
        std::string_view agent_id,
        size_t max_keep = 1000
    ) -> size_t;

    /**
     * @brief Clear working memories
     *
     * Delete all working memories for specified session
     *
     * @param session_id Session ID
     * @return Number of cleared memories
     */
    auto clear_working_memories(std::string_view session_id) -> size_t;

    // ========== Statistics ==========

    /**
     * @brief Get total memory count
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    /**
     * @brief Get Agent's memory count
     */
    [[nodiscard]] auto get_agent_memory_count(std::string_view agent_id) const -> size_t;

    /**
     * @brief Clear all memories
     */
    auto clear() noexcept -> void;

private:
    MemoryManager() = default;

    /**
     * @brief Generate memory ID
     */
    [[nodiscard]] auto generate_memory_id() const -> std::string;

    /**
     * @brief Update indexes
     */
    auto update_indexes(const MemoryEntry& memory) -> void;

    /**
     * @brief Remove from indexes
     */
    auto remove_from_indexes(const MemoryEntry& memory) -> void;

    // Thread safety: read-write lock
    mutable std::shared_mutex mutex_;

    // Main storage: memory_id -> MemoryEntry
    std::unordered_map<std::string, MemoryEntry> memory_store_;

    // Index: owner_agent_id -> memory_id list
    std::unordered_multimap<std::string, std::string> owner_index_;

    // Index: tag -> memory_id list
    std::unordered_multimap<std::string, std::string> tag_index_;

    // Index: task_id -> memory_id list
    std::unordered_map<std::string, std::vector<std::string>> task_index_;

    // Shared context: context_id -> participant_ids
    std::unordered_map<std::string, std::vector<std::string>> shared_contexts_;
};

// Global singleton access macro
#define MEMORY_MANAGER moltcat::memory::MemoryManager::get_instance()

} // namespace moltcat::memory
