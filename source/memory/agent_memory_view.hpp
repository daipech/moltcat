#pragma once

#include "memory_manager.hpp"
#include <string>
#include <vector>
#include <memory>

namespace moltcat::memory {

/**
 * @brief Agent memory view
 *
 * Provides isolated memory access interface, automatically handles shared and private memories
 *
 * Design principles:
 * - Each Agent instance owns a MemoryView
 * - View filters memories without access permission
 * - Simplifies Agent's memory operation interface
 * - Supports temporary context memory (session level)
 */
class AgentMemoryView {
public:
    /**
     * @brief Constructor
     *
     * @param agent_id Agent ID
     * @param manager Memory manager reference
     */
    AgentMemoryView(
        std::string agent_id,
        MemoryManager& manager
    );

    /**
     * @brief Destructor
     */
    ~AgentMemoryView() = default;

    // ========== Private Memory Operations ==========

    /**
     * @brief Store private memory
     *
     * Create memory with PRIVATE visibility, only accessible to creator
     *
     * @param memory Memory entry (visibility will be forced to PRIVATE)
     * @return Memory ID
     */
    [[nodiscard]] auto store_private(const MemoryEntry& memory) -> std::string;

    /**
     * @brief Read private memory
     *
     * @param memory_id Memory ID
     * @return Memory entry (if exists and is private memory)
     */
    [[nodiscard]] auto get_private(std::string_view memory_id) const
        -> std::optional<MemoryEntry>;

    /**
     * @brief Get all private memories
     *
     * @return Private memory list
     */
    [[nodiscard]] auto list_private() const -> std::vector<MemoryEntry>;

    // ========== Shared Memory Operations (Read-only) ==========

    /**
     * @brief Read shared memory
     *
     * @param memory_id Memory ID
     * @return Memory entry (if exists and has permission)
     */
    [[nodiscard]] auto get_shared(std::string_view memory_id) const
        -> std::optional<MemoryEntry>;

    /**
     * @brief Get all shared memories
     *
     * @return Shared memory list (SHARED + GLOBAL)
     */
    [[nodiscard]] auto list_shared() const -> std::vector<MemoryEntry>;

    /**
     * @brief Search shared memories (based on tags)
     *
     * @param tags Tag list
     * @return Matching memory list
     */
    [[nodiscard]] auto search_shared_by_tags(
        const std::vector<std::string>& tags
    ) const -> std::vector<MemoryEntry>;

    // ========== Global Memory Operations (Read-only) ==========

    /**
     * @brief Get all global memories
     *
     * @return Global memory list (visibility=GLOBAL)
     */
    [[nodiscard]] auto list_global() const -> std::vector<MemoryEntry>;

    // ========== General Operations ==========

    /**
     * @brief Store memory (auto-set owner)
     *
     * @param memory Memory entry
     * @return Memory ID
     */
    [[nodiscard]] auto store(const MemoryEntry& memory) -> std::string;

    /**
     * @brief Read memory (auto permission check)
     *
     * @param memory_id Memory ID
     * @return Memory entry (if exists and has permission)
     */
    [[nodiscard]] auto get(std::string_view memory_id) const
        -> std::optional<MemoryEntry>;

    /**
     * @brief Get all visible memories
     *
     * @return All accessible memories (PRIVATE + SHARED + GLOBAL)
     */
    [[nodiscard]] auto list_all() const -> std::vector<MemoryEntry>;

    /**
     * @brief Share private memory with other Agent
     *
     * @param memory_id Memory ID
     * @param target_agent_id Target Agent ID
     * @return Whether successful
     */
    auto share_with(std::string_view memory_id, std::string_view target_agent_id) -> bool;

    /**
     * @brief Delete memory
     *
     * Can only delete memories owned by self
     *
     * @param memory_id Memory ID
     * @return Whether successful
     */
    auto delete_memory(std::string_view memory_id) -> bool;

    // ========== Context Memory (Temporary Sharing) ==========

    /**
     * @brief Add context memory
     *
     * Add memory to temporary context for session-level information sharing
     *
     * @param memory_id Memory ID
     */
    auto add_context_memory(std::string_view memory_id) -> void;

    /**
     * @brief Get context memory list
     *
     * @return Context memory ID list
     */
    [[nodiscard]] auto get_context_memories() const
        -> const std::vector<std::string>&;

    /**
     * @brief Get context memory content
     *
     * @return Context memory content list
     */
    [[nodiscard]] auto get_context_memory_entries() const
        -> std::vector<MemoryEntry>;

    /**
     * @brief Clear context memories
     */
    auto clear_context_memories() -> void;

    // ========== Semantic Search ==========

    /**
     * @brief Semantic search (private + shared + global)
     *
     * @param query_embedding Query vector
     * @param top_k Return top K results
     * @return Matching memory list
     */
    [[nodiscard]] auto semantic_search(
        std::string_view query_embedding,
        size_t top_k = 10
    ) const -> std::vector<MemoryEntry>;

    /**
     * @brief Tag search (private + shared + global)
     *
     * @param tags Tag list
     * @return Matching memory list
     */
    [[nodiscard]] auto search_by_tags(
        const std::vector<std::string>& tags
    ) const -> std::vector<MemoryEntry>;

    /**
     * @brief Task-related memories
     *
     * @param task_id Task ID
     * @return All memories related to this task
     */
    [[nodiscard]] auto get_task_memories(std::string_view task_id) const
        -> std::vector<MemoryEntry>;

    // ========== Statistics ==========

    /**
     * @brief Get private memory count
     */
    [[nodiscard]] auto private_count() const -> size_t;

    /**
     * @brief Get shared memory count
     */
    [[nodiscard]] auto shared_count() const -> size_t;

    /**
     * @brief Get global memory count
     */
    [[nodiscard]] auto global_count() const -> size_t;

    /**
     * @brief Get Agent ID
     */
    [[nodiscard]] auto get_agent_id() const noexcept -> const std::string& {
        return agent_id_;
    }

private:
    std::string agent_id_;              // Agent ID
    MemoryManager& manager_;            // Memory manager reference
    std::vector<std::string> context_memories_; // Context memory ID list
};

} // namespace moltcat::memory
