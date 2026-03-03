#pragma once

#include "a2a_types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <glaze/json.hpp>

namespace moltcat::protocol::extensions {

// ================================
// MoltCat Standard Extension URIs
// ================================

/**
 * MoltCat Extension URI namespace
 *
 * Format: urn:moltcat:extension:<category>:<name>
 *
 * Standard extension list:
 * - urn:moltcat:extension:memory:query    - Memory query
 * - urn:moltcat:extension:memory:context  - Memory context
 * - urn:moltcat:extension:task:priority   - Task priority
 * - urn:moltcat:extension:task:timeout    - Task timeout
 */
namespace uris {
    constexpr auto MEMORY_QUERY = "urn:moltcat:extension:memory:query";
    constexpr auto MEMORY_CONTEXT = "urn:moltcat:extension:memory:context";
    constexpr auto TASK_PRIORITY = "urn:moltcat:extension:task:priority";
    constexpr auto TASK_TIMEOUT = "urn:moltcat:extension:task:timeout";
}

// ================================
// Memory Query Extension
// ================================

/**
 * Memory query request
 *
 * Passed via A2A Message.extensions, indicates Agent needs to retrieve relevant memories
 */
struct MemoryQueryExtension {
    std::string query;                               // Query text
    size_t top_k = 5;                                // Number of results to return
    float threshold = 0.7f;                          // Similarity threshold [0.0, 1.0]
    std::vector<std::string> memory_types;          // Memory type filter (optional)
    std::string agent_id;                            // Agent ID initiating the query

    /**
     * Serialize to JSON (for extensions storage)
     */
    auto serialize() const -> glz::json_t {
        return glz::json_t{
            {"query", query},
            {"top_k", top_k},
            {"threshold", threshold},
            {"memory_types", memory_types},
            {"agent_id", agent_id}
        };
    }

    /**
     * Deserialize from JSON
     */
    static auto deserialize(const glz::json_t& json) -> std::optional<MemoryQueryExtension> {
        try {
            MemoryQueryExtension ext;
            ext.query = json.at("query").get<std::string>();
            ext.top_k = json.value("top_k", 5);
            ext.threshold = json.value("threshold", 0.7f);
            ext.memory_types = json.value("memory_types", std::vector<std::string>{});
            ext.agent_id = json.value("agent_id", "");
            return ext;
        } catch (...) {
            return std::nullopt;
        }
    }
};

/**
 * Memory query result
 *
 * Retrieval results returned to Agent, passed via Message.metadata
 */
struct MemoryQueryResult {
    struct MemoryHit {
        std::string memory_id;
        std::string content;
        float score;                                  // Similarity score
        std::string memory_type;
        uint64_t timestamp;
    };

    std::vector<MemoryHit> hits;                     // Retrieved memories
    std::string query;                               // Original query
    size_t total_count;                              // Total matches

    auto serialize() const -> glz::json_t {
        std::vector<glz::json_t> hits_json;
        for (const auto& hit : hits) {
            hits_json.push_back(glz::json_t{
                {"memory_id", hit.memory_id},
                {"content", hit.content},
                {"score", hit.score},
                {"memory_type", hit.memory_type},
                {"timestamp", hit.timestamp}
            });
        }
        return glz::json_t{
            {"hits", hits_json},
            {"query", query},
            {"total_count", total_count}
        };
    }
};

// ================================
// Memory Context Extension
// ================================

/**
 * Memory context
 *
 * Pass retrieved memories as context to Agent
 */
struct MemoryContextExtension {
    struct ContextMemory {
        std::string memory_id;
        std::string content;
        std::string memory_type;                     // EPISODIC/SEMANTIC/PROCEDURAL/WORKING
        float relevance_score;                       // Relevance to current task
    };

    std::vector<ContextMemory> memories;            // Relevant memory list
    std::string context_type;                        // Context type (e.g., "task_relevant")

    auto serialize() const -> glz::json_t {
        std::vector<glz::json_t> memories_json;
        for (const auto& mem : memories) {
            memories_json.push_back(glz::json_t{
                {"memory_id", mem.memory_id},
                {"content", mem.content},
                {"memory_type", mem.memory_type},
                {"relevance_score", mem.relevance_score}
            });
        }
        return glz::json_t{
            {"memories", memories_json},
            {"context_type", context_type}
        };
    }

    static auto deserialize(const glz::json_t& json) -> std::optional<MemoryContextExtension> {
        try {
            MemoryContextExtension ext;
            ext.context_type = json.value("context_type", "task_relevant");

            auto memories_json = json.at("memories");
            for (const auto& mem_json : memories_json) {
                ContextMemory mem;
                mem.memory_id = mem_json.at("memory_id").get<std::string>();
                mem.content = mem_json.at("content").get<std::string>();
                mem.memory_type = mem_json.at("memory_type").get<std::string>();
                mem.relevance_score = mem_json.at("relevance_score").get<float>();
                ext.memories.push_back(mem);
            }
            return ext;
        } catch (...) {
            return std::nullopt;
        }
    }
};

// ================================
// Extension Builder
// ================================

/**
 * MemoryExtensionBuilder - Helper to build memory-related extensions for A2A Messages
 */
class MemoryExtensionBuilder {
public:
    /**
     * Add memory query request to Message
     *
     * @param msg A2A Message
     * @param query Query text
     * @param top_k Number of results to return
     */
    static auto add_memory_query(a2a::Message& msg,
                                  std::string_view query,
                                  size_t top_k = 5) -> void
    {
        MemoryQueryExtension ext;
        ext.query = query;
        ext.top_k = top_k;
        ext.threshold = 0.7f;
        ext.agent_id = msg.metadata.value("agent_id", "");

        // Add to extensions list
        msg.extensions.push_back(uris::MEMORY_QUERY);

        // Serialize to metadata
        msg.metadata["memory_query"] = ext.serialize();
    }

    /**
     * Add memory context to Message
     *
     * @param msg A2A Message
     * @param memories Relevant memory list
     */
    static auto add_memory_context(a2a::Message& msg,
                                    const std::vector<MemoryContextExtension::ContextMemory>& memories) -> void
    {
        MemoryContextExtension ext;
        ext.memories = memories;
        ext.context_type = "task_relevant";

        // Add to extensions list
        msg.extensions.push_back(uris::MEMORY_CONTEXT);

        // Serialize to metadata
        msg.metadata["memory_context"] = ext.serialize();
    }

    /**
     * Extract memory query request from Message
     */
    static auto get_memory_query(const a2a::Message& msg)
        -> std::optional<MemoryQueryExtension>
    {
        // Check if contains memory_query extension
        auto it = std::find(msg.extensions.begin(), msg.extensions.end(), uris::MEMORY_QUERY);
        if (it == msg.extensions.end()) {
            return std::nullopt;
        }

        // Deserialize from metadata
        if (!msg.metadata.contains("memory_query")) {
            return std::nullopt;
        }

        return MemoryQueryExtension::deserialize(msg.metadata.at("memory_query"));
    }

    /**
     * Extract memory context from Message
     */
    static auto get_memory_context(const a2a::Message& msg)
        -> std::optional<MemoryContextExtension>
    {
        // Check if contains memory_context extension
        auto it = std::find(msg.extensions.begin(), msg.extensions.end(), uris::MEMORY_CONTEXT);
        if (it == msg.extensions.end()) {
            return std::nullopt;
        }

        // Deserialize from metadata
        if (!msg.metadata.contains("memory_context")) {
            return std::nullopt;
        }

        return MemoryContextExtension::deserialize(msg.metadata.at("memory_context"));
    }

    /**
     * Check if Message contains memory-related extension
     */
    static auto has_memory_extension(const a2a::Message& msg) -> bool {
        return std::any_of(msg.extensions.begin(), msg.extensions.end(),
            [](const std::string& ext) {
                return ext == uris::MEMORY_QUERY || ext == uris::MEMORY_CONTEXT;
            });
    }
};

// ================================
// Memory System Integration Adapter
// ================================

/**
 * MemoryA2AAdapter - Adapter layer between memory system and A2A protocol
 *
 * Responsibilities:
 * 1. Convert memory queries to A2A Message extensions
 * 2. Extract memory requests from A2A Messages
 * 3. Wrap retrieval results as A2A Messages
 */
class MemoryA2AAdapter {
public:
    /**
     * Construct A2A Message with memory query
     *
     * @param query_text Query text
     * @param context_id Session ID
     * @param agent_id Agent ID
     * @param top_k Number of results to return
     * @return A2A Message (contains memory query extension)
     */
    static auto create_memory_query_message(
        std::string_view query_text,
        std::string_view context_id,
        std::string_view agent_id,
        size_t top_k = 5
    ) -> a2a::Message
    {
        auto msg = a2a::Message::create_user_message(query_text, context_id);
        msg.metadata["agent_id"] = std::string(agent_id);

        MemoryExtensionBuilder::add_memory_query(msg, query_text, top_k);

        return msg;
    }

    /**
     * Construct A2A Message with memory context
     *
     * @param user_message Original user message
     * @param memories Relevant memory list
     * @return A2A Message (contains memory context extension)
     */
    static auto enrich_with_memory_context(
        a2a::Message& user_message,
        const std::vector<MemoryContextExtension::ContextMemory>& memories
    ) -> void
    {
        if (!memories.empty()) {
            MemoryExtensionBuilder::add_memory_context(user_message, memories);
        }
    }

    /**
     * Convert memory retrieval result to A2A Artifact
     *
     * @param query_result Retrieval result
     * @return A2A Artifact
     */
    static auto to_memory_result_artifact(const MemoryQueryResult& query_result)
        -> a2a::Artifact
    {
        a2a::Artifact artifact;
        artifact.artifact_id = utils::UUID::generate_v4();
        artifact.name = "memory_retrieval_result";

        // Convert retrieval result to text Part
        std::string result_text = "Retrieved " + std::to_string(query_result.hits.size()) + " relevant memories:\n";
        for (const auto& hit : query_result.hits) {
            result_text += "- [" + hit.memory_type + "] " + hit.content +
                          " (similarity: " + std::to_string(hit.score) + ")\n";
        }

        artifact.parts.push_back(a2a::Part::from_text(result_text));

        // Store complete result in metadata
        artifact.metadata["memory_query_result"] = query_result.serialize();

        return artifact;
    }
};

} // namespace moltcat::protocol::extensions

// ================================
// Glaze Serialization Support
// ================================

template <>
struct glz::meta<moltcat::protocol::extensions::MemoryQueryExtension> {
    using T = moltcat::protocol::extensions::MemoryQueryExtension;
    static constexpr auto value = glz::object(
        "query", &T::query,
        "top_k", &T::top_k,
        "threshold", &T::threshold,
        "memory_types", &T::memory_types,
        "agent_id", &T::agent_id
    );
};

template <>
struct glz::meta<moltcat::protocol::extensions::MemoryContextExtension> {
    using T = moltcat::protocol::extensions::MemoryContextExtension;
    static constexpr auto value = glz::object(
        "memories", &T::memories,
        "context_type", &T::context_type
    );
};
