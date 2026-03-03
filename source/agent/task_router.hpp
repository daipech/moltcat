#pragma once

#include <string>
#include <vector>
#include "task_types.hpp"
#include "task_classifier.hpp"

namespace moltcat::agent {

// Forward declarations
class AgentTemplateRegistry;

/**
 * Routing decision result
 *
 * Contains selected Agent list, collaboration mode, and decision reasoning
 */
struct RoutingDecision {
    std::vector<std::string> selected_agent_ids;  // Selected Agent ID list
    CollaborationMode mode;                        // Collaboration mode
    std::string reasoning;                         // Decision reasoning (for debugging)
};

/**
 * Task router
 *
 * Responsibilities:
 * - Select appropriate Agents based on task classification
 * - Select appropriate collaboration mode based on task classification
 * - Support intelligent routing based on capabilities
 *
 * Implementation:
 * - Simple routing based on task type (Phase 1)
 * - Future upgrade to intelligent routing based on Agent capability matrix (Phase 2)
 */
class TaskRouter {
public:
    /**
     * @brief Routing decision (based on task classification)
     *
     * Select appropriate Agents from available Agents based on task classification, and determine collaboration mode
     *
     * @param classification Task classification
     * @param available_agents Available Agent ID list
     * @return Routing decision
     */
    [[nodiscard]] static auto route(
        const TaskClassification& classification,
        const std::vector<std::string>& available_agents
    ) -> RoutingDecision;

    /**
     * @brief Intelligent routing (based on Agent capabilities)
     *
     * Intelligently select most appropriate Agents based on task type and Agent capabilities
     *
     * @param task Task
     * @param agent_registry Agent registry (query capabilities)
     * @return Routing decision
     */
    [[nodiscard]] static auto intelligent_route(
        const model::Task& task,
        const AgentTemplateRegistry& agent_registry
    ) -> RoutingDecision;

    /**
     * @brief Select Agents based on task type
     *
     * Example: CODE_REVIEW tasks should be assigned to Agents with "code-review" capability
     *
     * @param category Task category
     * @param available_agents Available Agent ID list
     * @return Selected Agent ID list
     */
    [[nodiscard]] static auto select_agents_by_category(
        TaskCategory category,
        const std::vector<std::string>& available_agents
    ) -> std::vector<std::string>;

    /**
     * @brief Select collaboration mode based on task type
     *
     * @param category Task category
     * @return Collaboration mode
     */
    [[nodiscard]] static auto select_mode_by_category(
        TaskCategory category
    ) -> CollaborationMode;

private:
    /**
     * @brief Generate decision reasoning
     *
     * @param category Task category
     * @param selected_agents Number of selected Agents
     * @param mode Collaboration mode
     * @return Decision reasoning
     */
    [[nodiscard]] static auto generate_reasoning(
        TaskCategory category,
        size_t selected_agents,
        CollaborationMode mode
    ) -> std::string;

    /**
     * @brief Task type to capability mapping table
     *
     * Mapping: task category -> required capability list
     */
    static const std::unordered_map<TaskCategory, std::vector<std::string>> CAPABILITY_MAP;
};

} // namespace moltcat::agent
