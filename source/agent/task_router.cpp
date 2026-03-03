#include "task_router.hpp"
#include "agent_template_registry.hpp"
#include <algorithm>
#include <sstream>

namespace moltcat::agent {

// ========== Capability Mapping Table ==========

const std::unordered_map<TaskCategory, std::vector<std::string>>
TaskRouter::CAPABILITY_MAP = {
    {TaskCategory::CODE_REVIEW,   {"code-review", "analysis", "static-analysis"}},
    {TaskCategory::DEBUGGING,     {"debugging", "bug-fixing", "troubleshooting"}},
    {TaskCategory::TESTING,       {"testing", "unit-test", "integration-test"}},
    {TaskCategory::DOCUMENTATION, {"documentation", "writing", "explanation"}},
    {TaskCategory::REFACTORING,   {"refactoring", "code-cleanup", "restructuring"}},
    {TaskCategory::OPTIMIZATION,  {"optimization", "performance", "profiling"}},
    {TaskCategory::GENERATION,    {"code-generation", "implementation", "development"}},
    {TaskCategory::UNKNOWN,       {"general", "default"}}
};

// ========== Public Method Implementations ==========

auto TaskRouter::route(
    const TaskClassification& classification,
    const std::vector<std::string>& available_agents
) -> RoutingDecision {
    // 1. Select Agents
    std::vector<std::string> selected_agents =
        select_agents_by_category(classification.category, available_agents);

    // 2. Select collaboration mode
    CollaborationMode mode = classification.recommended_mode;

    // 3. Generate decision reasoning
    std::string reasoning = generate_reasoning(
        classification.category,
        selected_agents.size(),
        mode
    );

    return RoutingDecision{
        .selected_agent_ids = std::move(selected_agents),
        .mode = mode,
        .reasoning = std::move(reasoning)
    };
}

auto TaskRouter::intelligent_route(
    const model::Task& task,
    const AgentTemplateRegistry& agent_registry
) -> RoutingDecision {
    // 1. Classify task
    auto classification = TaskClassifier::classify(task);

    // 2. Get all available Agent IDs
    auto available_agents = agent_registry.get_all_agent_ids();

    // 3. Filter Agents based on capabilities (future implementation)
    // Currently returns all Agents, can filter based on capability matrix later

    // 4. Call basic routing
    return route(classification, available_agents);
}

auto TaskRouter::select_agents_by_category(
    TaskCategory category,
    const std::vector<std::string>& available_agents
) -> std::vector<std::string> {
    // Phase 1 implementation: return all available Agents
    // Future Phase 2 can filter based on Agent capability matrix

    if (available_agents.empty()) {
        return {};
    }

    // Determine number of Agents to select based on task type
    size_t count = 0;

    switch (category) {
        case TaskCategory::CODE_REVIEW:
            // Code review: up to 3 Agents (different perspectives)
            count = std::min(size_t(3), available_agents.size());
            break;

        case TaskCategory::DEBUGGING:
            // Debugging: 2-3 Agents (competitive mode)
            count = std::min(size_t(3), available_agents.size());
            break;

        case TaskCategory::TESTING:
            // Testing: up to 5 Agents (parallel execution)
            count = std::min(size_t(5), available_agents.size());
            break;

        case TaskCategory::DOCUMENTATION:
            // Documentation generation: 1-2 Agents
            count = std::min(size_t(2), available_agents.size());
            break;

        case TaskCategory::REFACTORING:
            // Refactoring: 2-3 Agents (pipeline mode)
            count = std::min(size_t(3), available_agents.size());
            break;

        case TaskCategory::OPTIMIZATION:
            // Performance optimization: 2-4 Agents (competitive mode)
            count = std::min(size_t(4), available_agents.size());
            break;

        case TaskCategory::GENERATION:
            // Code generation: 2-3 Agents (competitive mode)
            count = std::min(size_t(3), available_agents.size());
            break;

        case TaskCategory::UNKNOWN:
        default:
            // Unknown type: use all Agents
            count = available_agents.size();
            break;
    }

    // Return first count Agents
    std::vector<std::string> selected;
    selected.reserve(count);

    for (size_t i = 0; i < count && i < available_agents.size(); ++i) {
        selected.push_back(available_agents[i]);
    }

    return selected;
}

auto TaskRouter::select_mode_by_category(TaskCategory category)
    -> CollaborationMode {
    // Delegate to TaskClassifier's recommendation logic
    return TaskClassifier::recommend_mode(category);
}

// ========== Private Method Implementations ==========

auto TaskRouter::generate_reasoning(
    TaskCategory category,
    size_t selected_agents,
    CollaborationMode mode
) -> std::string {
    std::ostringstream oss;

    oss << "Task type: " << TaskClassifier::category_to_string(category);
    oss << ", selected " << selected_agents << " Agents";
    oss << ", collaboration mode: " << collaboration_mode_to_string(mode);

    // Add mode selection reasoning
    switch (mode) {
        case CollaborationMode::PIPELINE:
            oss << " (Pipeline mode: suitable for sequential refactoring tasks)";
            break;
        case CollaborationMode::PARALLEL:
            oss << " (Parallel mode: suitable for simultaneous review/testing tasks)";
            break;
        case CollaborationMode::COMPETITIVE:
            oss << " (Competitive mode: suitable for optimization/debugging tasks requiring optimal solution)";
            break;
        case CollaborationMode::HIERARCHICAL:
            oss << " (Hierarchical mode: suitable for documentation generation tasks with master-slave relationship)";
            break;
    }

    return oss.str();
}

} // namespace moltcat::agent
