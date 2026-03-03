#pragma once

#include <cstdint>

namespace moltcat::agent {

/**
 * Collaboration mode enumeration
 *
 * Defines the collaboration mode of multiple Agents in AgentTeam
 */
enum class CollaborationMode : uint8_t {
    PIPELINE,      // Pipeline (sequential processing)
    PARALLEL,      // Parallel (simultaneous processing)
    COMPETITIVE,   // Competitive (first to finish wins)
    HIERARCHICAL   // Hierarchical (master-slave mode)
};

/**
 * Convert collaboration mode to string
 */
inline auto collaboration_mode_to_string(CollaborationMode mode)
    -> std::string_view {
    switch (mode) {
        case CollaborationMode::PIPELINE:     return "PIPELINE";
        case CollaborationMode::PARALLEL:     return "PARALLEL";
        case CollaborationMode::COMPETITIVE:  return "COMPETITIVE";
        case CollaborationMode::HIERARCHICAL: return "HIERARCHICAL";
        default:                              return "UNKNOWN";
    }
};

} // namespace moltcat::agent
