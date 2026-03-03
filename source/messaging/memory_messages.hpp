#pragma once

#include "message.hpp"
#include "source/memory/memory_manager.hpp"
#include <glaze/json.hpp>
#include <optional>
#include <vector>
#include <string>

namespace moltcat::messaging::memory {

// ================================
// Memory Store Request
// ================================

/**
 * @brief Memory storage request
 *
 * Used for Agent to send storage requests to MemoryService
 */
struct MemoryStoreRequest {
    std::string agent_id;
    ::moltcat::memory::MemoryEntry memory;
    bool broadcast_change = true;

    /**
     * @brief Serialize to JSON (using Glaze)
     */
    [[nodiscard]] auto serialize() const -> glz::json_t {
        return glz::json_t{
            {"agent_id", agent_id},
            {"memory", glz::json_t{
                {"memory_id", memory.memory_id},
                {"owner_agent_id", memory.owner_agent_id},
                {"memory_type", static_cast<int>(memory.memory_type)},
                {"visibility", static_cast<int>(memory.visibility)},
                {"title", memory.title},
                {"content", memory.content},
                {"embedding", memory.embedding},
                {"tags", memory.tags},
                {"referenced_task_ids", memory.referenced_task_ids},
                {"access_count", memory.access_count},
                {"last_access_time", memory.last_access_time},
                {"created_at", memory.created_at},
                {"updated_at", memory.updated_at},
                {"metadata", memory.metadata}
            }},
            {"broadcast_change", broadcast_change}
        };
    }

    /**
     * @brief Deserialize from JSON
     */
    static auto deserialize(const glz::json_t& json) -> std::optional<MemoryStoreRequest> {
        try {
            MemoryStoreRequest req;

            if (json.contains("agent_id") && json["agent_id"].is_string()) {
                req.agent_id = json["agent_id"].get<std::string>();
            } else {
                return std::nullopt;
            }

            if (json.contains("memory") && json["memory"].is_object()) {
                const auto& mem_json = json["memory"];

                // 解析 MemoryEntry
                ::moltcat::memory::MemoryEntry mem;

                if (mem_json.contains("memory_id")) mem.memory_id = mem_json["memory_id"].get<std::string>();
                if (mem_json.contains("owner_agent_id")) mem.owner_agent_id = mem_json["owner_agent_id"].get<std::string>();
                if (mem_json.contains("memory_type")) mem.memory_type = static_cast<::moltcat::memory::MemoryType>(mem_json["memory_type"].get<int>());
                if (mem_json.contains("visibility")) mem.visibility = static_cast<::moltcat::memory::MemoryVisibility>(mem_json["visibility"].get<int>());
                if (mem_json.contains("title")) mem.title = mem_json["title"].get<std::string>();
                if (mem_json.contains("content")) mem.content = mem_json["content"].get<std::string>();
                if (mem_json.contains("embedding")) mem.embedding = mem_json["embedding"].get<std::vector<float>>();
                if (mem_json.contains("tags")) mem.tags = mem_json["tags"].get<std::vector<std::string>>();
                if (mem_json.contains("referenced_task_ids")) mem.referenced_task_ids = mem_json["referenced_task_ids"].get<std::vector<std::string>>();
                if (mem_json.contains("access_count")) mem.access_count = mem_json["access_count"].get<uint64_t>();
                if (mem_json.contains("last_access_time")) mem.last_access_time = mem_json["last_access_time"].get<uint64_t>();
                if (mem_json.contains("created_at")) mem.created_at = mem_json["created_at"].get<uint64_t>();
                if (mem_json.contains("updated_at")) mem.updated_at = mem_json["updated_at"].get<uint64_t>();
                if (mem_json.contains("metadata")) mem.metadata = mem_json["metadata"].get<std::unordered_map<std::string, std::string>>();

                req.memory = std::move(mem);
            } else {
                return std::nullopt;
            }

            if (json.contains("broadcast_change")) {
                req.broadcast_change = json["broadcast_change"].get<bool>();
            }

            return req;
        } catch (const std::exception& e) {
            return std::nullopt;
        }
    }
};

// ================================
// Memory Retrieve Request
// ================================

/**
 * @brief Memory retrieval request
 *
 * Used for Agent to retrieve specific MemoryEntry
 */
struct MemoryRetrieveRequest {
    std::string agent_id;
    std::string memory_id;

    [[nodiscard]] auto serialize() const -> glz::json_t {
        return glz::json_t{
            {"agent_id", agent_id},
            {"memory_id", memory_id}
        };
    }

    static auto deserialize(const glz::json_t& json) -> std::optional<MemoryRetrieveRequest> {
        try {
            MemoryRetrieveRequest req;

            if (!json.contains("agent_id") || !json["agent_id"].is_string()) {
                return std::nullopt;
            }
            req.agent_id = json["agent_id"].get<std::string>();

            if (!json.contains("memory_id") || !json["memory_id"].is_string()) {
                return std::nullopt;
            }
            req.memory_id = json["memory_id"].get<std::string>();

            return req;
        } catch (const std::exception& e) {
            return std::nullopt;
        }
    }
};

// ================================
// Memory Query Request
// ================================

/**
 * @brief Memory query request
 *
 * Supports semantic search, tag search, task-related search
 */
struct MemoryQueryRequest {
    enum class QueryType {
        SEMANTIC,       // Semantic search (vector similarity)
        TAGS,           // Tag search
        TASK_RELATED    // Task-related search
    };

    std::string agent_id;
    QueryType query_type;
    std::string query_text;
    std::vector<std::string> tags;
    size_t top_k = 10;

    [[nodiscard]] auto serialize() const -> glz::json_t {
        return glz::json_t{
            {"agent_id", agent_id},
            {"query_type", static_cast<int>(query_type)},
            {"query_text", query_text},
            {"tags", tags},
            {"top_k", top_k}
        };
    }

    static auto deserialize(const glz::json_t& json) -> std::optional<MemoryQueryRequest> {
        try {
            MemoryQueryRequest req;

            if (!json.contains("agent_id") || !json["agent_id"].is_string()) {
                return std::nullopt;
            }
            req.agent_id = json["agent_id"].get<std::string>();

            if (json.contains("query_type") && json["query_type"].is_number_integer()) {
                req.query_type = static_cast<QueryType>(json["query_type"].get<int>());
            }

            if (json.contains("query_text")) {
                req.query_text = json["query_text"].get<std::string>();
            }

            if (json.contains("tags")) {
                req.tags = json["tags"].get<std::vector<std::string>>();
            }

            if (json.contains("top_k")) {
                req.top_k = json["top_k"].get<size_t>();
            }

            return req;
        } catch (const std::exception& e) {
            return std::nullopt;
        }
    }
};

// ================================
// Memory Batch Request
// ================================

/**
 * @brief Memory batch operation request
 *
 * Supports batch storage, retrieval, deletion, sharing
 */
struct MemoryBatchRequest {
    enum class BatchOperation {
        STORE_MULTI,      // Batch storage
        RETRIEVE_MULTI,   // Batch retrieval
        DELETE_MULTI,     // Batch deletion
        SHARE_MULTI       // Batch sharing
    };

    std::string agent_id;
    BatchOperation operation;
    std::vector<::moltcat::memory::MemoryEntry> memories;
    std::vector<std::string> memory_ids;
    std::vector<std::string> target_agent_ids;  // For SHARE_MULTI

    [[nodiscard]] auto serialize() const -> glz::json_t {
        glz::json_t mem_array = glz::json_t::array();
        for (const auto& mem : memories) {
            mem_array.push_back(glz::json_t{
                {"memory_id", mem.memory_id},
                {"owner_agent_id", mem.owner_agent_id},
                {"memory_type", static_cast<int>(mem.memory_type)},
                {"visibility", static_cast<int>(mem.visibility)},
                {"title", mem.title},
                {"content", mem.content},
                {"embedding", mem.embedding},
                {"tags", mem.tags},
                {"referenced_task_ids", mem.referenced_task_ids},
                {"access_count", mem.access_count},
                {"last_access_time", mem.last_access_time},
                {"created_at", mem.created_at},
                {"updated_at", mem.updated_at},
                {"metadata", mem.metadata}
            });
        }

        return glz::json_t{
            {"agent_id", agent_id},
            {"operation", static_cast<int>(operation)},
            {"memories", mem_array},
            {"memory_ids", memory_ids},
            {"target_agent_ids", target_agent_ids}
        };
    }

    static auto deserialize(const glz::json_t& json) -> std::optional<MemoryBatchRequest> {
        try {
            MemoryBatchRequest req;

            if (!json.contains("agent_id") || !json["agent_id"].is_string()) {
                return std::nullopt;
            }
            req.agent_id = json["agent_id"].get<std::string>();

            if (json.contains("operation") && json["operation"].is_number_integer()) {
                req.operation = static_cast<BatchOperation>(json["operation"].get<int>());
            }

            if (json.contains("memories") && json["memories"].is_array()) {
                for (const auto& mem_json : json["memories"]) {
                    ::moltcat::memory::MemoryEntry mem;

                    if (mem_json.contains("memory_id")) mem.memory_id = mem_json["memory_id"].get<std::string>();
                    if (mem_json.contains("owner_agent_id")) mem.owner_agent_id = mem_json["owner_agent_id"].get<std::string>();
                    if (mem_json.contains("memory_type")) mem.memory_type = static_cast<::moltcat::memory::MemoryType>(mem_json["memory_type"].get<int>());
                    if (mem_json.contains("visibility")) mem.visibility = static_cast<::moltcat::memory::MemoryVisibility>(mem_json["visibility"].get<int>());
                    if (mem_json.contains("title")) mem.title = mem_json["title"].get<std::string>();
                    if (mem_json.contains("content")) mem.content = mem_json["content"].get<std::string>();
                    if (mem_json.contains("embedding")) mem.embedding = mem_json["embedding"].get<std::vector<float>>();
                    if (mem_json.contains("tags")) mem.tags = mem_json["tags"].get<std::vector<std::string>>();
                    if (mem_json.contains("referenced_task_ids")) mem.referenced_task_ids = mem_json["referenced_task_ids"].get<std::vector<std::string>>();
                    if (mem_json.contains("access_count")) mem.access_count = mem_json["access_count"].get<uint64_t>();
                    if (mem_json.contains("last_access_time")) mem.last_access_time = mem_json["access_count"].get<uint64_t>();
                    if (mem_json.contains("created_at")) mem.created_at = mem_json["created_at"].get<uint64_t>();
                    if (mem_json.contains("updated_at")) mem.updated_at = mem_json["updated_at"].get<uint64_t>();
                    if (mem_json.contains("metadata")) mem.metadata = mem_json["metadata"].get<std::unordered_map<std::string, std::string>>();

                    req.memories.push_back(std::move(mem));
                }
            }

            if (json.contains("memory_ids")) {
                req.memory_ids = json["memory_ids"].get<std::vector<std::string>>();
            }

            if (json.contains("target_agent_ids")) {
                req.target_agent_ids = json["target_agent_ids"].get<std::vector<std::string>>();
            }

            return req;
        } catch (const std::exception& e) {
            return std::nullopt;
        }
    }
};

// ================================
// Memory Share Request
// ================================

/**
 * @brief Memory sharing request
 *
 * Used for Agent to share their Memory with other Agents
 */
struct MemoryShareRequest {
    std::string agent_id;
    std::string memory_id;
    std::string target_agent_id;
    bool allow_modify = false;  // Whether to allow target Agent to modify

    [[nodiscard]] auto serialize() const -> glz::json_t {
        return glz::json_t{
            {"agent_id", agent_id},
            {"memory_id", memory_id},
            {"target_agent_id", target_agent_id},
            {"allow_modify", allow_modify}
        };
    }

    static auto deserialize(const glz::json_t& json) -> std::optional<MemoryShareRequest> {
        try {
            MemoryShareRequest req;

            if (!json.contains("agent_id") || !json["agent_id"].is_string()) {
                return std::nullopt;
            }
            req.agent_id = json["agent_id"].get<std::string>();

            if (!json.contains("memory_id") || !json["memory_id"].is_string()) {
                return std::nullopt;
            }
            req.memory_id = json["memory_id"].get<std::string>();

            if (!json.contains("target_agent_id") || !json["target_agent_id"].is_string()) {
                return std::nullopt;
            }
            req.target_agent_id = json["target_agent_id"].get<std::string>();

            if (json.contains("allow_modify")) {
                req.allow_modify = json["allow_modify"].get<bool>();
            }

            return req;
        } catch (const std::exception& e) {
            return std::nullopt;
        }
    }
};

// ================================
// Memory Response (Generic Response)
// ================================

/**
 * @brief Memory operation response
 *
 * Unified response format for all Memory requests
 */
struct MemoryResponse {
    bool success = false;
    std::string error_message;
    std::optional<::moltcat::memory::MemoryEntry> memory;
    std::vector<::moltcat::memory::MemoryEntry> memories;
    std::string memory_id;  // ID returned by store/delete operations
    std::vector<std::string> memory_ids;  // ID list returned by batch operations
    bool operation_result = false;  // Boolean result for share/delete operations

    [[nodiscard]] auto serialize() const -> glz::json_t {
        glz::json_t json{
            {"success", success},
            {"error_message", error_message},
            {"operation_result", operation_result}
        };

        if (memory_id.has_value()) {
            json["memory_id"] = memory_id.value();
        }

        if (!memory_ids.empty()) {
            json["memory_ids"] = memory_ids;
        }

        if (memory.has_value()) {
            const auto& mem = memory.value();
            json["memory"] = glz::json_t{
                {"memory_id", mem.memory_id},
                {"owner_agent_id", mem.owner_agent_id},
                {"memory_type", static_cast<int>(mem.memory_type)},
                {"visibility", static_cast<int>(mem.visibility)},
                {"title", mem.title},
                {"content", mem.content},
                {"embedding", mem.embedding},
                {"tags", mem.tags},
                {"referenced_task_ids", mem.referenced_task_ids},
                {"access_count", mem.access_count},
                {"last_access_time", mem.last_access_time},
                {"created_at", mem.created_at},
                {"updated_at", mem.updated_at},
                {"metadata", mem.metadata}
            };
        }

        if (!memories.empty()) {
            glz::json_t mem_array = glz::json_t::array();
            for (const auto& mem : memories) {
                mem_array.push_back(glz::json_t{
                    {"memory_id", mem.memory_id},
                    {"owner_agent_id", mem.owner_agent_id},
                    {"memory_type", static_cast<int>(mem.memory_type)},
                    {"visibility", static_cast<int>(mem.visibility)},
                    {"title", mem.title},
                    {"content", mem.content},
                    {"embedding", mem.embedding},
                    {"tags", mem.tags},
                    {"referenced_task_ids", mem.referenced_task_ids},
                    {"access_count", mem.access_count},
                    {"last_access_time", mem.last_access_time},
                    {"created_at", mem.created_at},
                    {"updated_at", mem.updated_at},
                    {"metadata", mem.metadata}
                });
            }
            json["memories"] = mem_array;
        }

        return json;
    }

    static auto deserialize(const glz::json_t& json) -> std::optional<MemoryResponse> {
        try {
            MemoryResponse resp;

            if (json.contains("success")) {
                resp.success = json["success"].get<bool>();
            }

            if (json.contains("error_message")) {
                resp.error_message = json["error_message"].get<std::string>();
            }

            if (json.contains("memory_id")) {
                resp.memory_id = json["memory_id"].get<std::string>();
            }

            if (json.contains("memory_ids")) {
                resp.memory_ids = json["memory_ids"].get<std::vector<std::string>>();
            }

            if (json.contains("operation_result")) {
                resp.operation_result = json["operation_result"].get<bool>();
            }

            if (json.contains("memory") && json["memory"].is_object()) {
                const auto& mem_json = json["memory"];
                ::moltcat::memory::MemoryEntry mem;

                if (mem_json.contains("memory_id")) mem.memory_id = mem_json["memory_id"].get<std::string>();
                if (mem_json.contains("owner_agent_id")) mem.owner_agent_id = mem_json["owner_agent_id"].get<std::string>();
                if (mem_json.contains("memory_type")) mem.memory_type = static_cast<::moltcat::memory::MemoryType>(mem_json["memory_type"].get<int>());
                if (mem_json.contains("visibility")) mem.visibility = static_cast<::moltcat::memory::MemoryVisibility>(mem_json["visibility"].get<int>());
                if (mem_json.contains("title")) mem.title = mem_json["title"].get<std::string>();
                if (mem_json.contains("content")) mem.content = mem_json["content"].get<std::string>();
                if (mem_json.contains("embedding")) mem.embedding = mem_json["embedding"].get<std::vector<float>>();
                if (mem_json.contains("tags")) mem.tags = mem_json["tags"].get<std::vector<std::string>>();
                if (mem_json.contains("referenced_task_ids")) mem.referenced_task_ids = mem_json["referenced_task_ids"].get<std::vector<std::string>>();
                if (mem_json.contains("access_count")) mem.access_count = mem_json["access_count"].get<uint64_t>();
                if (mem_json.contains("last_access_time")) mem.last_access_time = mem_json["last_access_time"].get<uint64_t>();
                if (mem_json.contains("created_at")) mem.created_at = mem_json["created_at"].get<uint64_t>();
                if (mem_json.contains("updated_at")) mem.updated_at = mem_json["updated_at"].get<uint64_t>();
                if (mem_json.contains("metadata")) mem.metadata = mem_json["metadata"].get<std::unordered_map<std::string, std::string>>();

                resp.memory = std::move(mem);
            }

            if (json.contains("memories") && json["memories"].is_array()) {
                for (const auto& mem_json : json["memories"]) {
                    ::moltcat::memory::MemoryEntry mem;

                    if (mem_json.contains("memory_id")) mem.memory_id = mem_json["memory_id"].get<std::string>();
                    if (mem_json.contains("owner_agent_id")) mem.owner_agent_id = mem_json["owner_agent_id"].get<std::string>();
                    if (mem_json.contains("memory_type")) mem.memory_type = static_cast<::moltcat::memory::MemoryType>(mem_json["memory_type"].get<int>());
                    if (mem_json.contains("visibility")) mem.visibility = static_cast<::moltcat::memory::MemoryVisibility>(mem_json["visibility"].get<int>());
                    if (mem_json.contains("title")) mem.title = mem_json["title"].get<std::string>();
                    if (mem_json.contains("content")) mem.content = mem_json["content"].get<std::string>();
                    if (mem_json.contains("embedding")) mem.embedding = mem_json["embedding"].get<std::vector<float>>();
                    if (mem_json.contains("tags")) mem.tags = mem_json["tags"].get<std::vector<std::string>>();
                    if (mem_json.contains("referenced_task_ids")) mem.referenced_task_ids = mem_json["referenced_task_ids"].get<std::vector<std::string>>();
                    if (mem_json.contains("access_count")) mem.access_count = mem_json["access_count"].get<uint64_t>();
                    if (mem_json.contains("last_access_time")) mem.last_access_time = mem_json["last_access_time"].get<uint64_t>();
                    if (mem_json.contains("created_at")) mem.created_at = mem_json["created_at"].get<uint64_t>();
                    if (mem_json.contains("updated_at")) mem.updated_at = mem_json["updated_at"].get<uint64_t>();
                    if (mem_json.contains("metadata")) mem.metadata = mem_json["metadata"].get<std::unordered_map<std::string, std::string>>();

                    resp.memories.push_back(std::move(mem));
                }
            }

            return resp;
        } catch (const std::exception& e) {
            return std::nullopt;
        }
    }
};

// ================================
// Memory Changed Event (PUB-SUB)
// ================================

/**
 * @brief Memory change event
 *
 * Broadcast Memory creation, update, deletion, sharing events via PUB-SUB pattern
 */
struct MemoryChangedEvent {
    enum class ChangeType {
        CREATED,
        UPDATED,
        DELETED,
        SHARED
    };

    std::string memory_id;
    std::string owner_agent_id;
    ChangeType change_type;
    uint64_t timestamp;
    std::optional<::moltcat::memory::MemoryEntry> memory_data;

    [[nodiscard]] auto serialize() const -> glz::json_t {
        glz::json_t json{
            {"memory_id", memory_id},
            {"owner_agent_id", owner_agent_id},
            {"change_type", static_cast<int>(change_type)},
            {"timestamp", timestamp}
        };

        if (memory_data.has_value()) {
            const auto& mem = memory_data.value();
            json["memory_data"] = glz::json_t{
                {"memory_id", mem.memory_id},
                {"owner_agent_id", mem.owner_agent_id},
                {"memory_type", static_cast<int>(mem.memory_type)},
                {"visibility", static_cast<int>(mem.visibility)},
                {"title", mem.title},
                {"content", mem.content},
                {"embedding", mem.embedding},
                {"tags", mem.tags},
                {"referenced_task_ids", mem.referenced_task_ids},
                {"access_count", mem.access_count},
                {"last_access_time", mem.last_access_time},
                {"created_at", mem.created_at},
                {"updated_at", mem.updated_at},
                {"metadata", mem.metadata}
            };
        }

        return json;
    }

    static auto deserialize(const glz::json_t& json) -> std::optional<MemoryChangedEvent> {
        try {
            MemoryChangedEvent event;

            if (!json.contains("memory_id") || !json["memory_id"].is_string()) {
                return std::nullopt;
            }
            event.memory_id = json["memory_id"].get<std::string>();

            if (!json.contains("owner_agent_id") || !json["owner_agent_id"].is_string()) {
                return std::nullopt;
            }
            event.owner_agent_id = json["owner_agent_id"].get<std::string>();

            if (!json.contains("change_type") || !json["change_type"].is_number_integer()) {
                return std::nullopt;
            }
            event.change_type = static_cast<ChangeType>(json["change_type"].get<int>());

            if (!json.contains("timestamp") || !json["timestamp"].is_number_unsigned()) {
                return std::nullopt;
            }
            event.timestamp = json["timestamp"].get<uint64_t>();

            if (json.contains("memory_data") && json["memory_data"].is_object()) {
                const auto& mem_json = json["memory_data"];
                ::moltcat::memory::MemoryEntry mem;

                if (mem_json.contains("memory_id")) mem.memory_id = mem_json["memory_id"].get<std::string>();
                if (mem_json.contains("owner_agent_id")) mem.owner_agent_id = mem_json["owner_agent_id"].get<std::string>();
                if (mem_json.contains("memory_type")) mem.memory_type = static_cast<::moltcat::memory::MemoryType>(mem_json["memory_type"].get<int>());
                if (mem_json.contains("visibility")) mem.visibility = static_cast<::moltcat::memory::MemoryVisibility>(mem_json["visibility"].get<int>());
                if (mem_json.contains("title")) mem.title = mem_json["title"].get<std::string>();
                if (mem_json.contains("content")) mem.content = mem_json["content"].get<std::string>();
                if (mem_json.contains("embedding")) mem.embedding = mem_json["embedding"].get<std::vector<float>>();
                if (mem_json.contains("tags")) mem.tags = mem_json["tags"].get<std::vector<std::string>>();
                if (mem_json.contains("referenced_task_ids")) mem.referenced_task_ids = mem_json["referenced_task_ids"].get<std::vector<std::string>>();
                if (mem_json.contains("access_count")) mem.access_count = mem_json["access_count"].get<uint64_t>();
                if (mem_json.contains("last_access_time")) mem.last_access_time = mem_json["last_access_time"].get<uint64_t>();
                if (mem_json.contains("created_at")) mem.created_at = mem_json["created_at"].get<uint64_t>();
                if (mem_json.contains("updated_at")) mem.updated_at = mem_json["updated_at"].get<uint64_t>();
                if (mem_json.contains("metadata")) mem.metadata = mem_json["metadata"].get<std::unordered_map<std::string, std::string>>();

                event.memory_data = std::move(mem);
            }

            return event;
        } catch (const std::exception& e) {
            return std::nullopt;
        }
    }
};

} // namespace moltcat::messaging::memory
