#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include "task_types.hpp"  // 包含 CollaborationMode 定义

namespace moltcat::agent {

/**
 * Task classification category
 *
 * Used to identify different types of development tasks for selecting appropriate Agents and collaboration modes
 */
enum class TaskCategory : uint8_t {
    UNKNOWN,         // Unknown type
    CODE_REVIEW,     // Code review
    DEBUGGING,       // Debugging
    TESTING,         // Testing
    DOCUMENTATION,   // Documentation generation
    REFACTORING,     // Refactoring
    OPTIMIZATION,    // Performance optimization
    GENERATION       // Code generation
};

/**
 * Task classification result
 *
 * Contains task type, confidence, tags, and recommended collaboration mode
 */
struct TaskClassification {
    TaskCategory category;                          // Task category
    float confidence;                               // Confidence [0.0, 1.0]
    std::vector<std::string> tags;                  // Extracted tags
    CollaborationMode recommended_mode;             // Recommended collaboration mode
    std::string reasoning;                          // Classification reasoning (for debugging)
};

/**
 * Task classifier
 *
 * Responsibilities:
 * - Analyze user instructions, extract task information
 * - Identify task type (CODE_REVIEW/DEBUGGING/TESTING, etc.)
 * - Recommend appropriate collaboration mode
 *
 * Implementation:
 * - Based on keyword matching (Phase 1)
 * - Future upgrade to LLM-based intelligent classification (Phase 2)
 */
class TaskClassifier {
public:
    /**
     * @brief Classify task (based on Task object)
     *
     * @param task Task object
     * @return Task classification result
     */
    [[nodiscard]] static auto classify(const model::Task& task)
        -> TaskClassification;

    /**
     * @brief Classify task (based on user instruction string)
     *
     * @param instruction User instruction (natural language)
     * @return Task classification result
     */
    [[nodiscard]] static auto classify_from_instruction(
        std::string_view instruction
    ) -> TaskClassification;

    /**
     * @brief Convert TaskCategory to string
     */
    [[nodiscard]] static auto category_to_string(TaskCategory category)
        -> std::string_view;

private:
    /**
     * @brief Keyword mapping table
     *
     * Mapping: keyword -> task category
     */
    static const std::unordered_map<std::string, TaskCategory> KEYWORD_MAP;

    /**
     * @brief Collaboration mode recommendation rules
     *
     * Recommend most appropriate collaboration mode based on task category
     *
     * @param category Task category
     * @return Recommended collaboration mode
     */
    [[nodiscard]] static auto recommend_mode(TaskCategory category)
        -> CollaborationMode;

    /**
     * @brief Extract tags from instruction
     *
     * Example: "Fix Bug #123" -> ["bug", "123"]
     *
     * @param instruction User instruction
     * @return Extracted tag list
     */
    [[nodiscard]] static auto extract_tags(std::string_view instruction)
        -> std::vector<std::string>;

    /**
     * @brief Calculate confidence
     *
     * Based on number of matched keywords and weights
     *
     * @param matched_keywords Number of matched keywords
     * @param total_keywords Total number of keywords
     * @return Confidence [0.0, 1.0]
     */
    [[nodiscard]] static auto calculate_confidence(
        size_t matched_keywords,
        size_t total_keywords
    ) -> float;

    /**
     * @brief Generate classification reasoning
     *
     * @param category Task category
     * @param matched_keywords Matched keywords
     * @return Classification reasoning
     */
    [[nodiscard]] static auto generate_reasoning(
        TaskCategory category,
        const std::vector<std::string>& matched_keywords
    ) -> std::string;
};

} // namespace moltcat::agent
