#include "agent_memory_view.hpp"
#include "utils/logger.hpp"
#include <algorithm>

namespace moltcat::memory {

AgentMemoryView::AgentMemoryView(
    std::string agent_id,
    MemoryManager& manager
)
    : agent_id_(std::move(agent_id))
    , manager_(manager)
    , context_memories_()
{
    MOLT_LOGGER.info("Created Agent memory view: agent={}", agent_id_);
}

// ========== Private Memory Operations ==========

auto AgentMemoryView::store_private(const MemoryEntry& memory) -> std::string {
    MemoryEntry private_memory = memory;

    // Set owner
    private_memory.owner_agent_id = agent_id_;

    // Force to private
    private_memory.visibility = MemoryVisibility::PRIVATE;

    // Clear allowed list (not needed for private memory)
    private_memory.allowed_readers.clear();

    return manager_.store_memory(private_memory);
}

auto AgentMemoryView::get_private(std::string_view memory_id) const
    -> std::optional<MemoryEntry> {

    auto memory = manager_.retrieve_memory(memory_id, agent_id_);
    if (!memory) {
        return std::nullopt;
    }

    // Verify is private memory
    if (memory->visibility != MemoryVisibility::PRIVATE ||
        memory->owner_agent_id != agent_id_) {

        MOLT_LOGGER.warn("Memory {} is not private memory of Agent {}",
                         memory_id, agent_id_);
        return std::nullopt;
    }

    return memory;
}

auto AgentMemoryView::list_private() const -> std::vector<MemoryEntry> {
    auto all_memories = manager_.get_visible_memories(agent_id_);

    std::vector<MemoryEntry> private_memories;
    for (auto& memory : all_memories) {
        if (memory.visibility == MemoryVisibility::PRIVATE &&
            memory.owner_agent_id == agent_id_) {

            private_memories.push_back(std::move(memory));
        }
    }

    MOLT_LOGGER.debug("Agent {} private memory count: {}", agent_id_, private_memories.size());
    return private_memories;
}

// ========== Shared Memory Operations ==========

auto AgentMemoryView::get_shared(std::string_view memory_id) const
    -> std::optional<MemoryEntry> {

    auto memory = manager_.retrieve_memory(memory_id, agent_id_);
    if (!memory) {
        return std::nullopt;
    }

    // Verify is shared or global memory
    if (memory->visibility == MemoryVisibility::PRIVATE) {
        MOLT_LOGGER.warn("Memory {} is private memory, cannot be accessed as shared memory",
                         memory_id);
        return std::nullopt;
    }

    return memory;
}

auto AgentMemoryView::list_shared() const -> std::vector<MemoryEntry> {
    auto all_memories = manager_.get_visible_memories(agent_id_);

    std::vector<MemoryEntry> shared_memories;
    for (auto& memory : all_memories) {
        if (memory.visibility == MemoryVisibility::SHARED ||
            memory.visibility == MemoryVisibility::GLOBAL) {

            shared_memories.push_back(std::move(memory));
        }
    }

    MOLT_LOGGER.debug("Agent {} shared memory count: {}", agent_id_, shared_memories.size());
    return shared_memories;
}

auto AgentMemoryView::search_shared_by_tags(
    const std::vector<std::string>& tags
) const -> std::vector<MemoryEntry> {

    auto results = manager_.search_by_tags(tags, agent_id_);

    // Filter out private memories
    results.erase(
        std::remove_if(results.begin(), results.end(),
                      [](const MemoryEntry& m) {
                          return m.visibility == MemoryVisibility::PRIVATE;
                      }),
        results.end()
    );

    return results;
}

// ========== Global Memory Operations ==========

auto AgentMemoryView::list_global() const -> std::vector<MemoryEntry> {
    auto all_memories = manager_.get_visible_memories(agent_id_);

    std::vector<MemoryEntry> global_memories;
    for (auto& memory : all_memories) {
        if (memory.visibility == MemoryVisibility::GLOBAL) {
            global_memories.push_back(std::move(memory));
        }
    }

    MOLT_LOGGER.debug("Agent {} global memory count: {}", agent_id_, global_memories.size());
    return global_memories;
}

// ========== General Operations ==========

auto AgentMemoryView::store(const MemoryEntry& memory) -> std::string {
    MemoryEntry new_memory = memory;

    // Set owner
    if (new_memory.owner_agent_id.empty()) {
        new_memory.owner_agent_id = agent_id_;
    }

    // Verify owner
    if (new_memory.owner_agent_id != agent_id_) {
        MOLT_LOGGER.error("Failed to store memory: owner mismatch (expected={}, got={})",
                          agent_id_, new_memory.owner_agent_id);
        return "";
    }

    return manager_.store_memory(new_memory);
}

auto AgentMemoryView::get(std::string_view memory_id) const
    -> std::optional<MemoryEntry> {

    return manager_.retrieve_memory(memory_id, agent_id_);
}

auto AgentMemoryView::list_all() const -> std::vector<MemoryEntry> {
    return manager_.get_visible_memories(agent_id_);
}

auto AgentMemoryView::share_with(
    std::string_view memory_id,
    std::string_view target_agent_id
) -> bool {

    return manager_.share_memory(memory_id, agent_id_, target_agent_id);
}

auto AgentMemoryView::delete_memory(std::string_view memory_id) -> bool {
    return manager_.delete_memory(memory_id, agent_id_);
}

// ========== Context Memory ==========

auto AgentMemoryView::add_context_memory(std::string_view memory_id) -> void {
    // Verify memory exists and has permission
    auto memory = manager_.retrieve_memory(memory_id, agent_id_);
    if (!memory) {
        MOLT_LOGGER.warn("Failed to add context memory: memory does not exist or no permission {}", memory_id);
        return;
    }

    // Add to context list (avoid duplicates)
    if (std::find(context_memories_.begin(), context_memories_.end(), memory_id)
        == context_memories_.end()) {

        context_memories_.emplace_back(memory_id);
        MOLT_LOGGER.debug("Added context memory: {} (agent={})", memory_id, agent_id_);
    }
}

auto AgentMemoryView::get_context_memories() const
    -> const std::vector<std::string>& {

    return context_memories_;
}

auto AgentMemoryView::get_context_memory_entries() const
    -> std::vector<MemoryEntry> {

    std::vector<MemoryEntry> results;
    results.reserve(context_memories_.size());

    for (const auto& memory_id : context_memories_) {
        if (auto memory = manager_.retrieve_memory(memory_id, agent_id_)) {
            results.push_back(std::move(*memory));
        }
    }

    return results;
}

auto AgentMemoryView::clear_context_memories() -> void {
    context_memories_.clear();
    MOLT_LOGGER.debug("Cleared context memories (agent={})", agent_id_);
}

// ========== Semantic Search ==========

auto AgentMemoryView::semantic_search(
    std::string_view query_embedding,
    size_t top_k
) const -> std::vector<MemoryEntry> {

    return manager_.semantic_search(query_embedding, agent_id_, top_k);
}

auto AgentMemoryView::search_by_tags(
    const std::vector<std::string>& tags
) const -> std::vector<MemoryEntry> {

    return manager_.search_by_tags(tags, agent_id_);
}

auto AgentMemoryView::get_task_memories(std::string_view task_id) const
    -> std::vector<MemoryEntry> {

    return manager_.get_task_memories(task_id, agent_id_);
}

// ========== Statistics ==========

auto AgentMemoryView::private_count() const -> size_t {
    auto all = list_all();

    return std::count_if(all.begin(), all.end(),
                         [this](const MemoryEntry& m) {
                             return m.visibility == MemoryVisibility::PRIVATE &&
                                    m.owner_agent_id == agent_id_;
                         });
}

auto AgentMemoryView::shared_count() const -> size_t {
    auto all = list_all();

    return std::count_if(all.begin(), all.end(),
                         [](const MemoryEntry& m) {
                             return m.visibility == MemoryVisibility::SHARED;
                         });
}

auto AgentMemoryView::global_count() const -> size_t {
    auto all = list_all();

    return std::count_if(all.begin(), all.end(),
                         [](const MemoryEntry& m) {
                             return m.visibility == MemoryVisibility::GLOBAL;
                         });
}

} // namespace moltcat::memory
