#pragma once

#include "types.hpp"
#include <glaze/glaze.hpp>
#include <string>
#include <vector>
#include <map>

namespace moltcat::model {

/**
 * @brief Agent static configuration
 *
 * Configuration set at creation time, immutable during runtime
 * All fields are read-only (should not be modified after initialization)
 */
struct AgentConfig {
    // ========== Basic Information ==========
    std::string agent_id;                      // Agent unique ID
    std::string name;                          // Agent name
    std::string description;                   // Description

    // ========== Provider Configuration ==========
    std::string provider_id;                   // Provider ID (references Provider)

    // ========== Resource Limits ==========
    uint64_t max_memory_bytes{2147483648};     // Maximum memory usage (default 2GB)
    uint64_t task_timeout_ms{30000};           // Default task timeout (milliseconds)

    // ========== Rate Limits ==========
    uint32_t requests_per_minute{50};          // Requests per minute limit
    uint32_t tokens_per_minute{100000};        // Tokens per minute limit

    // ========== Advanced Configuration ==========
    uint32_t retry_attempts{3};                // Failure retry attempts
    uint64_t connection_timeout_ms{10000};     // Connection timeout
    std::vector<std::string> allowed_capabilities; // Allowed capability list
};

/**
 * @brief Agent metadata
 *
 * Descriptive information, does not affect Agent operation
 */
struct AgentMetadata {
    uint64_t created_at{0};                    // Creation timestamp (milliseconds)
    uint64_t last_modified_at{0};              // Last modification timestamp
    std::string version{"1.0.0"};              // Agent version
    std::string author;                       // Creator/author
    std::vector<std::string> tags;             // Tags (for classification and search)
    std::map<std::string, std::string> custom_fields; // Custom fields

    // ========== Statistics (read-only) ==========
    uint64_t total_executions{0};              // Total executions
    uint64_t total_uptime_ms{0};               // Total uptime
    float average_response_time_ms{0.0f};      // Average response time
};

} // namespace moltcat::model

// Glaze serialization support - AgentConfig
template <>
struct glz::meta<moltcat::model::AgentConfig> {
    using T = moltcat::model::AgentConfig;
    static constexpr auto value = glz::object(
        "agent_id",                &T::agent_id,
        "name",                    &T::name,
        "description",             &T::description,
        "provider_id",             &T::provider_id,
        "max_memory_bytes",        &T::max_memory_bytes,
        "task_timeout_ms",         &T::task_timeout_ms,
        "requests_per_minute",     &T::requests_per_minute,
        "tokens_per_minute",       &T::tokens_per_minute,
        "retry_attempts",          &T::retry_attempts,
        "connection_timeout_ms",   &T::connection_timeout_ms,
        "allowed_capabilities",    &T::allowed_capabilities
    );
};

// Glaze serialization support - AgentMetadata
template <>
struct glz::meta<moltcat::model::AgentMetadata> {
    using T = moltcat::model::AgentMetadata;
    static constexpr auto value = glz::object(
        "created_at",              &T::created_at,
        "last_modified_at",        &T::last_modified_at,
        "version",                 &T::version,
        "author",                  &T::author,
        "tags",                    &T::tags,
        "custom_fields",           &T::custom_fields,
        "total_executions",        &T::total_executions,
        "total_uptime_ms",         &T::total_uptime_ms,
        "average_response_time_ms",&T::average_response_time_ms
    );
};
