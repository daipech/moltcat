#include "memory_manager.hpp"
#include "utils/logger.hpp"
#include "utils/uuid.hpp"
#include <algorithm>
#include <stdexcept>
#include <format>
#include <fmt/ranges.h>

namespace moltcat::memory {

auto MemoryManager::get_instance() -> MemoryManager& {
    static MemoryManager instance;
    return instance;
}

// ========== Memory Write ==========

auto MemoryManager::store_memory(const MemoryEntry& memory) -> std::string {
    MemoryEntry entry = memory;

    // Generate memory_id (if empty)
    if (entry.memory_id.empty()) {
        entry.memory_id = generate_memory_id();
    }

    // Set creation time (if 0)
    if (entry.created_at == 0) {
        entry.created_at = MemoryEntry::get_current_time_ms();
        entry.last_accessed_at = entry.created_at;
    }

    // Validate required fields
    if (entry.owner_agent_id.empty()) {
        MOLT_LOGGER.error("Failed to store memory: owner_agent_id is empty");
        return "";
    }

    std::unique_lock lock(mutex_);

    // Check if already exists
    if (memory_store_.find(entry.memory_id) != memory_store_.end()) {
        MOLT_LOGGER.warn("Memory ID already exists, will overwrite: {}", entry.memory_id);
    }

    // Store memory
    memory_store_[entry.memory_id] = entry;

    // Update indexes
    update_indexes(entry);

    MOLT_LOGGER.info("Stored memory: {} (type={}, visibility={}, owner={})",
                     entry.memory_id,
                     static_cast<int>(entry.type),
                     static_cast<int>(entry.visibility),
                     entry.owner_agent_id);

    return entry.memory_id;
}

auto MemoryManager::store_memories(const std::vector<MemoryEntry>& memories)
    -> std::vector<std::string> {

    std::vector<std::string> memory_ids;
    memory_ids.reserve(memories.size());

    for (const auto& memory : memories) {
        auto id = store_memory(memory);
        if (!id.empty()) {
            memory_ids.push_back(id);
        }
    }

    MOLT_LOGGER.info("Batch stored memories: {} succeeded", memory_ids.size());
    return memory_ids;
}

// ========== Memory Read ==========

auto MemoryManager::retrieve_memory(
    std::string_view memory_id,
    std::string_view agent_id
) const -> std::optional<MemoryEntry> {

    std::shared_lock lock(mutex_);

    auto it = memory_store_.find(std::string(memory_id));
    if (it == memory_store_.end()) {
        MOLT_LOGGER.debug("Memory does not exist: {}", memory_id);
        return std::nullopt;
    }

    const auto& entry = it->second;

    // Permission check
    if (!entry.is_accessible_by(agent_id)) {
        MOLT_LOGGER.warn("Agent {} has no permission to access memory {}", agent_id, memory_id);
        return std::nullopt;
    }

    // Update access statistics (needs const_cast, because mutex is shared_lock)
    const_cast<MemoryEntry&>(entry).update_access_stats();

    MOLT_LOGGER.debug("Agent {} retrieved memory {}", agent_id, memory_id);
    return entry;
}

auto MemoryManager::retrieve_memories(
    const std::vector<std::string>& memory_ids,
    std::string_view agent_id
) const -> std::vector<MemoryEntry> {

    std::vector<MemoryEntry> results;
    results.reserve(memory_ids.size());

    for (const auto& id : memory_ids) {
        if (auto memory = retrieve_memory(id, agent_id)) {
            results.push_back(std::move(*memory));
        }
    }

    MOLT_LOGGER.debug("Agent {} batch retrieved memories: {} succeeded", agent_id, results.size());
    return results;
}

// ========== Memory Search ==========

auto MemoryManager::semantic_search(
    std::string_view query_embedding,
    std::string_view agent_id,
    size_t top_k,
    float min_similarity
) const -> std::vector<MemoryEntry> {

    // Phase 2: Simplified implementation, returns all accessible memories
    // Actual vector retrieval will be implemented in later phases (needs integration of embedding model)

    std::shared_lock lock(mutex_);

    std::vector<MemoryEntry> results;

    for (const auto& [id, entry] : memory_store_) {
        // Permission check
        if (!entry.is_accessible_by(agent_id)) {
            continue;
        }

        // TODO: Actual vector similarity calculation
        // Current simplified implementation: skip memories without embedding
        if (entry.embedding.empty()) {
            continue;
        }

        results.push_back(entry);

        // Limit result count
        if (results.size() >= top_k) {
            break;
        }
    }

    MOLT_LOGGER.info("Semantic search: query={}, agent={}, result_count={}",
                     query_embedding, agent_id, results.size());

    return results;
}

auto MemoryManager::search_by_tags(
    const std::vector<std::string>& tags,
    std::string_view agent_id
) const -> std::vector<MemoryEntry> {

    if (tags.empty()) {
        return {};
    }

    std::shared_lock lock(mutex_);

    // Collect all matching memory_ids
    std::unordered_set<std::string> candidate_ids;

    // First tag: initialize candidate set
    {
        auto range = tag_index_.equal_range(tags[0]);
        for (auto it = range.first; it != range.second; ++it) {
            candidate_ids.insert(it->second);
        }
    }

    // Subsequent tags: take intersection (AND relationship)
    for (size_t i = 1; i < tags.size(); ++i) {
        std::unordered_set<std::string> temp;

        auto range = tag_index_.equal_range(tags[i]);
        for (auto it = range.first; it != range.second; ++it) {
            if (candidate_ids.contains(it->second)) {
                temp.insert(it->second);
            }
        }

        candidate_ids = std::move(temp);

        // Early exit: if candidate set is empty
        if (candidate_ids.empty()) {
            return {};
        }
    }

    // Build results
    std::vector<MemoryEntry> results;
    for (const auto& id : candidate_ids) {
        auto it = memory_store_.find(id);
        if (it != memory_store_.end()) {
            const auto& entry = it->second;

            // Permission check
            if (!entry.is_accessible_by(agent_id)) {
                continue;
            }

            results.push_back(entry);
        }
    }

    MOLT_LOGGER.info("Tag search: tags={}, agent={}, result_count={}",
                     fmt::join(tags, ","), agent_id, results.size());

    return results;
}

auto MemoryManager::get_task_memories(
    std::string_view task_id,
    std::string_view agent_id
) const -> std::vector<MemoryEntry> {

    std::shared_lock lock(mutex_);

    std::vector<MemoryEntry> results;

    auto it = task_index_.find(std::string(task_id));
    if (it == task_index_.end()) {
        return results;
    }

    for (const auto& memory_id : it->second) {
        auto mem_it = memory_store_.find(memory_id);
        if (mem_it != memory_store_.end()) {
            const auto& entry = mem_it->second;

            // Permission check
            if (!entry.is_accessible_by(agent_id)) {
                continue;
            }

            results.push_back(entry);
        }
    }

    MOLT_LOGGER.info("Task memory retrieval: task={}, agent={}, result_count={}",
                     task_id, agent_id, results.size());

    return results;
}

auto MemoryManager::get_visible_memories(std::string_view agent_id) const
    -> std::vector<MemoryEntry> {

    std::shared_lock lock(mutex_);

    std::vector<MemoryEntry> results;

    for (const auto& [id, entry] : memory_store_) {
        if (entry.is_accessible_by(agent_id)) {
            results.push_back(entry);
        }
    }

    MOLT_LOGGER.debug("Get visible memories: agent={}, count={}", agent_id, results.size());
    return results;
}

// ========== Memory Sharing ==========

auto MemoryManager::share_memory(
    std::string_view memory_id,
    std::string_view owner_agent_id,
    std::string_view target_agent_id
) -> bool {

    std::unique_lock lock(mutex_);

    auto it = memory_store_.find(std::string(memory_id));
    if (it == memory_store_.end()) {
        MOLT_LOGGER.error("Failed to share memory: memory does not exist {}", memory_id);
        return false;
    }

    auto& entry = it->second;

    // Permission check: only owner can share
    if (entry.owner_agent_id != owner_agent_id) {
        MOLT_LOGGER.error("Failed to share memory: Agent {} is not the owner of memory {}",
                          owner_agent_id, memory_id);
        return false;
    }

    // Update visibility
    if (entry.visibility == MemoryVisibility::PRIVATE) {
        entry.visibility = MemoryVisibility::SHARED;
    }

    // Add target Agent to allowed list
    if (std::find(entry.allowed_readers.begin(), entry.allowed_readers.end(), target_agent_id)
        == entry.allowed_readers.end()) {
        entry.allowed_readers.emplace_back(target_agent_id);
    }

    MOLT_LOGGER.info("Shared memory: {} from {} to {}",
                     memory_id, owner_agent_id, target_agent_id);

    return true;
}

auto MemoryManager::share_memory_with_multiple(
    std::string_view memory_id,
    std::string_view owner_agent_id,
    const std::vector<std::string>& target_agent_ids
) -> size_t {

    size_t success_count = 0;

    for (const auto& target_id : target_agent_ids) {
        if (share_memory(memory_id, owner_agent_id, target_id)) {
            ++success_count;
        }
    }

    MOLT_LOGGER.info("Batch share memory: {} -> {} Agents",
                     memory_id, success_count);

    return success_count;
}

auto MemoryManager::create_shared_context(
    const std::vector<std::string>& participant_ids,
    std::string_view context_name
) -> std::string {

    if (participant_ids.empty()) {
        MOLT_LOGGER.error("Failed to create shared context: participant list is empty");
        return "";
    }

    // Generate context ID
    std::string context_id = "ctx_" + utils::generate_uuid();

    std::unique_lock lock(mutex_);
    shared_contexts_[context_id] = participant_ids;

    MOLT_LOGGER.info("Created shared context: {} (participants={})",
                     context_id, participant_ids.size());

    return context_id;
}

// ========== Memory Management ==========

auto MemoryManager::update_access_stats(std::string_view memory_id) -> bool {
    std::unique_lock lock(mutex_);

    auto it = memory_store_.find(std::string(memory_id));
    if (it == memory_store_.end()) {
        return false;
    }

    it->second.update_access_stats();
    return true;
}

auto MemoryManager::delete_memory(
    std::string_view memory_id,
    std::string_view requesting_agent_id
) -> bool {

    std::unique_lock lock(mutex_);

    auto it = memory_store_.find(std::string(memory_id));
    if (it == memory_store_.end()) {
        MOLT_LOGGER.warn("Failed to delete memory: memory does not exist {}", memory_id);
        return false;
    }

    const auto& entry = it->second;

    // Permission check: only owner can delete
    if (entry.owner_agent_id != requesting_agent_id) {
        MOLT_LOGGER.error("Failed to delete memory: Agent {} is not the owner of memory {}",
                          requesting_agent_id, memory_id);
        return false;
    }

    // Remove from indexes
    remove_from_indexes(entry);

    // Delete from main storage
    memory_store_.erase(it);

    MOLT_LOGGER.info("Deleted memory: {} (owner={})",
                     memory_id, requesting_agent_id);

    return true;
}

auto MemoryManager::archive_old_memories(
    uint64_t before_timestamp,
    std::string_view agent_id,
    size_t max_keep
) -> size_t {

    std::unique_lock lock(mutex_);

    // Find all memories meeting criteria
    std::vector<std::string> to_archive;

    auto range = owner_index_.equal_range(std::string(agent_id));
    for (auto it = range.first; it != range.second; ++it) {
        auto mem_it = memory_store_.find(it->second);
        if (mem_it != memory_store_.end()) {
            const auto& entry = mem_it->second;

            // Criteria: time earlier than threshold && low importance
            if (entry.created_at < before_timestamp &&
                entry.importance < 0.5f &&
                entry.type == MemoryType::WORKING) {

                to_archive.push_back(entry.memory_id);
            }
        }
    }

    // Sort by importance, keep most important
    std::sort(to_archive.begin(), to_archive.end(), [this](const auto& a, const auto& b) {
        return memory_store_[a].importance > memory_store_[b].importance;
    });

    // If exceeds max_keep, archive excess
    size_t archive_count = 0;
    if (to_archive.size() > max_keep) {
        to_archive.resize(to_archive.size() - max_keep);

        for (const auto& id : to_archive) {
            auto mem_it = memory_store_.find(id);
            if (mem_it != memory_store_.end()) {
                remove_from_indexes(mem_it->second);
                memory_store_.erase(mem_it);
                ++archive_count;
            }
        }
    }

    MOLT_LOGGER.info("Archived old memories: agent={}, timestamp<={}, archived_count={}",
                     agent_id, before_timestamp, archive_count);

    return archive_count;
}

auto MemoryManager::clear_working_memories(std::string_view session_id) -> size_t {

    std::unique_lock lock(mutex_);

    size_t cleared_count = 0;

    for (auto it = memory_store_.begin(); it != memory_store_.end(); ) {
        const auto& entry = it->second;

        if (entry.session_id == session_id &&
            entry.type == MemoryType::WORKING) {

            remove_from_indexes(entry);
            it = memory_store_.erase(it);
            ++cleared_count;
        } else {
            ++it;
        }
    }

    MOLT_LOGGER.info("Cleared working memories: session={}, cleared_count={}", session_id, cleared_count);
    return cleared_count;
}

// ========== Statistics ==========

auto MemoryManager::size() const noexcept -> size_t {
    std::shared_lock lock(mutex_);
    return memory_store_.size();
}

auto MemoryManager::get_agent_memory_count(std::string_view agent_id) const -> size_t {
    std::shared_lock lock(mutex_);
    return owner_index_.count(std::string(agent_id));
}

auto MemoryManager::clear() noexcept -> void {
    std::unique_lock lock(mutex_);
    auto count = memory_store_.size();

    memory_store_.clear();
    owner_index_.clear();
    tag_index_.clear();
    task_index_.clear();
    shared_contexts_.clear();

    MOLT_LOGGER.info("Cleared all memories ({} entries)", count);
}

// ========== Private Methods ==========

auto MemoryManager::generate_memory_id() const -> std::string {
    return "mem_" + utils::generate_uuid();
}

auto MemoryManager::update_indexes(const MemoryEntry& memory) -> void {
    // Update owner index
    owner_index_.emplace(memory.owner_agent_id, memory.memory_id);

    // Update tag index
    for (const auto& tag : memory.tags) {
        tag_index_.emplace(tag, memory.memory_id);
    }

    // Update task index
    if (!memory.task_id.empty()) {
        task_index_[memory.task_id].push_back(memory.memory_id);
    }
}

auto MemoryManager::remove_from_indexes(const MemoryEntry& memory) -> void {
    // Remove from owner index
    auto owner_range = owner_index_.equal_range(memory.owner_agent_id);
    for (auto it = owner_range.first; it != owner_range.second; ) {
        if (it->second == memory.memory_id) {
            it = owner_index_.erase(it);
        } else {
            ++it;
        }
    }

    // Remove from tag index
    for (const auto& tag : memory.tags) {
        auto tag_range = tag_index_.equal_range(tag);
        for (auto it = tag_range.first; it != tag_range.second; ) {
            if (it->second == memory.memory_id) {
                it = tag_index_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Remove from task index
    if (!memory.task_id.empty()) {
        auto& task_memories = task_index_[memory.task_id];
        task_memories.erase(
            std::remove(task_memories.begin(), task_memories.end(), memory.memory_id),
            task_memories.end()
        );

        if (task_memories.empty()) {
            task_index_.erase(memory.task_id);
        }
    }
}

} // namespace moltcat::memory
