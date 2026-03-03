#include "task_classifier.hpp"
#include "model/task.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <regex>

namespace moltcat::agent {

// ========== Keyword Mapping Table ==========

const std::unordered_map<std::string, TaskCategory> TaskClassifier::KEYWORD_MAP = {
    // Code review keywords
    {"review", TaskCategory::CODE_REVIEW},
    {"审查", TaskCategory::CODE_REVIEW},
    {"audit", TaskCategory::CODE_REVIEW},
    {"检查", TaskCategory::CODE_REVIEW},
    {"inspect", TaskCategory::CODE_REVIEW},
    {"点评", TaskCategory::CODE_REVIEW},

    // Debugging keywords
    {"debug", TaskCategory::DEBUGGING},
    {"调试", TaskCategory::DEBUGGING},
    {"fix", TaskCategory::DEBUGGING},
    {"修复", TaskCategory::DEBUGGING},
    {"bug", TaskCategory::DEBUGGING},
    {"error", TaskCategory::DEBUGGING},
    {"错误", TaskCategory::DEBUGGING},
    {"issue", TaskCategory::DEBUGGING},
    {"问题", TaskCategory::DEBUGGING},
    {"crash", TaskCategory::DEBUGGING},
    {"崩溃", TaskCategory::DEBUGGING},

    // Testing keywords
    {"test", TaskCategory::TESTING},
    {"测试", TaskCategory::TESTING},
    {"verify", TaskCategory::TESTING},
    {"验证", TaskCategory::TESTING},
    {"unit test", TaskCategory::TESTING},
    {"单元测试", TaskCategory::TESTING},
    {"integration test", TaskCategory::TESTING},
    {"集成测试", TaskCategory::TESTING},

    // Documentation generation keywords
    {"document", TaskCategory::DOCUMENTATION},
    {"文档", TaskCategory::DOCUMENTATION},
    {"readme", TaskCategory::DOCUMENTATION},
    {"comment", TaskCategory::DOCUMENTATION},
    {"注释", TaskCategory::DOCUMENTATION},
    {"explain", TaskCategory::DOCUMENTATION},
    {"解释", TaskCategory::DOCUMENTATION},

    // Refactoring keywords
    {"refactor", TaskCategory::REFACTORING},
    {"重构", TaskCategory::REFACTORING},
    {"restructure", TaskCategory::REFACTORING},
    {"重组", TaskCategory::REFACTORING},
    {"clean", TaskCategory::REFACTORING},
    {"清理", TaskCategory::REFACTORING},
    {"simplify", TaskCategory::REFACTORING},
    {"简化", TaskCategory::REFACTORING},

    // Performance optimization keywords
    {"optimize", TaskCategory::OPTIMIZATION},
    {"优化", TaskCategory::OPTIMIZATION},
    {"performance", TaskCategory::OPTIMIZATION},
    {"性能", TaskCategory::OPTIMIZATION},
    {"speed", TaskCategory::OPTIMIZATION},
    {"速度", TaskCategory::OPTIMIZATION},
    {"efficiency", TaskCategory::OPTIMIZATION},
    {"效率", TaskCategory::OPTIMIZATION},
    {"memory", TaskCategory::OPTIMIZATION},
    {"内存", TaskCategory::OPTIMIZATION},

    // Code generation keywords
    {"generate", TaskCategory::GENERATION},
    {"生成", TaskCategory::GENERATION},
    {"implement", TaskCategory::GENERATION},
    {"实现", TaskCategory::GENERATION},
    {"create", TaskCategory::GENERATION},
    {"创建", TaskCategory::GENERATION},
    {"write", TaskCategory::GENERATION},
    {"编写", TaskCategory::GENERATION},
    {"develop", TaskCategory::GENERATION},
    {"开发", TaskCategory::GENERATION},
    {"build", TaskCategory::GENERATION},
    {"构建", TaskCategory::GENERATION}
};

// ========== Public Method Implementations ==========

auto TaskClassifier::classify(const model::Task& task)
    -> TaskClassification {
    // Extract text from Task's description and prompt
    std::string text = task.description;

    // Try to get prompt from payload
    if (task.payload.contains("prompt")) {
        if (auto prompt = task.payload["prompt"].get_if<std::string>(); prompt) {
            text += " " + *prompt;
        }
    }

    return classify_from_instruction(text);
}

auto TaskClassifier::classify_from_instruction(std::string_view instruction)
    -> TaskClassification {
    // 1. Convert instruction to lowercase for matching
    std::string lower_instruction;
    lower_instruction.reserve(instruction.size());
    for (char ch : instruction) {
        if (static_cast<unsigned char>(ch) < 128) {
            lower_instruction += static_cast<char>(std::tolower(ch));
        } else {
            lower_instruction += ch;  // Keep non-ASCII characters (like Chinese)
        }
    }

    // 2. Count matched keywords for each category
    std::unordered_map<TaskCategory, size_t> category_matches;
    std::unordered_map<TaskCategory, std::vector<std::string>> matched_keywords_map;

    for (const auto& [keyword, category] : KEYWORD_MAP) {
        std::string lower_keyword;
        lower_keyword.reserve(keyword.size());
        for (char ch : keyword) {
            if (static_cast<unsigned char>(ch) < 128) {
                lower_keyword += static_cast<char>(std::tolower(ch));
            } else {
                lower_keyword += ch;
            }
        }

        // Check if keyword appears in instruction
        if (lower_instruction.find(lower_keyword) != std::string::npos) {
            category_matches[category]++;
            matched_keywords_map[category].push_back(keyword);
        }
    }

    // 3. Select category with most matches
    TaskCategory best_category = TaskCategory::UNKNOWN;
    size_t max_matches = 0;
    std::vector<std::string> best_matched_keywords;

    for (const auto& [category, count] : category_matches) {
        if (count > max_matches) {
            max_matches = count;
            best_category = category;
            best_matched_keywords = matched_keywords_map[category];
        }
    }

    // 4. If no match, return UNKNOWN
    if (best_category == TaskCategory::UNKNOWN) {
        return TaskClassification{
            .category = TaskCategory::UNKNOWN,
            .confidence = 0.0f,
            .tags = extract_tags(instruction),
            .recommended_mode = CollaborationMode::PARALLEL,  // Default to parallel
            .reasoning = "No known keywords matched, defaulting to UNKNOWN type"
        };
    }

    // 5. Calculate confidence
    float confidence = calculate_confidence(max_matches, KEYWORD_MAP.size());

    // 6. Recommend collaboration mode
    CollaborationMode recommended_mode = recommend_mode(best_category);

    // 7. Generate classification reasoning
    std::string reasoning = generate_reasoning(best_category, best_matched_keywords);

    // 8. Extract tags
    std::vector<std::string> tags = extract_tags(instruction);

    return TaskClassification{
        .category = best_category,
        .confidence = confidence,
        .tags = std::move(tags),
        .recommended_mode = recommended_mode,
        .reasoning = std::move(reasoning)
    };
}

auto TaskClassifier::category_to_string(TaskCategory category)
    -> std::string_view {
    switch (category) {
        case TaskCategory::UNKNOWN:        return "UNKNOWN";
        case TaskCategory::CODE_REVIEW:    return "CODE_REVIEW";
        case TaskCategory::DEBUGGING:      return "DEBUGGING";
        case TaskCategory::TESTING:        return "TESTING";
        case TaskCategory::DOCUMENTATION:  return "DOCUMENTATION";
        case TaskCategory::REFACTORING:    return "REFACTORING";
        case TaskCategory::OPTIMIZATION:   return "OPTIMIZATION";
        case TaskCategory::GENERATION:     return "GENERATION";
        default:                           return "UNKNOWN";
    }
}

// ========== Private Method Implementations ==========

auto TaskClassifier::recommend_mode(TaskCategory category)
    -> CollaborationMode {
    switch (category) {
        case TaskCategory::CODE_REVIEW:
            // Code review: multiple Agents review in parallel, providing different perspectives
            return CollaborationMode::PARALLEL;

        case TaskCategory::DEBUGGING:
            // Debugging: competitive mode, fastest solution wins
            return CollaborationMode::COMPETITIVE;

        case TaskCategory::TESTING:
            // Testing: parallel mode, run multiple test cases simultaneously
            return CollaborationMode::PARALLEL;

        case TaskCategory::DOCUMENTATION:
            // Documentation: hierarchical mode, main Agent handles structure, sub-Agents handle details
            return CollaborationMode::HIERARCHICAL;

        case TaskCategory::REFACTORING:
            // Refactoring: pipeline mode, execute step by step (analyze -> refactor -> verify)
            return CollaborationMode::PIPELINE;

        case TaskCategory::OPTIMIZATION:
            // Performance optimization: competitive mode, multiple Agents try different optimization strategies
            return CollaborationMode::COMPETITIVE;

        case TaskCategory::GENERATION:
            // Code generation: competitive mode, select optimal implementation
            return CollaborationMode::COMPETITIVE;

        case TaskCategory::UNKNOWN:
        default:
            // Unknown type: default to parallel mode
            return CollaborationMode::PARALLEL;
    }
}

auto TaskClassifier::extract_tags(std::string_view instruction)
    -> std::vector<std::string> {
    std::vector<std::string> tags;

    // 1. Extract # tags (e.g., "fix #123" -> ["123"])
    static std::regex hash_tag_regex(R"(#(\w+))");
    std::string instruction_str(instruction);
    std::sregex_iterator it(instruction_str.begin(), instruction_str.end(), hash_tag_regex);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        tags.push_back(it->str(1));  // Capture group 1 (content after #)
    }

    // 2. Extract referenced filenames (e.g., "fix main.cpp" -> ["main.cpp"])
    static std::regex file_regex(R"([\w-]+\.(cpp|hpp|h|cc|cxx|py|js|ts|java|go|rs))");
    it = std::sregex_iterator(instruction_str.begin(), instruction_str.end(), file_regex);

    for (; it != end; ++it) {
        tags.push_back(it->str(0));
    }

    // 3. Extract numbers (possibly for Issue ID or PR ID)
    static std::regex number_regex(R"(\b\d{3,}\b)");  // At least 3 digits
    it = std::sregex_iterator(instruction_str.begin(), instruction_str.end(), number_regex);

    for (; it != end; ++it) {
        tags.push_back(it->str(0));
    }

    return tags;
}

auto TaskClassifier::calculate_confidence(
    size_t matched_keywords,
    size_t total_keywords
) -> float {
    if (total_keywords == 0) {
        return 0.0f;
    }

    // Base confidence: ratio of matched keywords
    float base_confidence = static_cast<float>(matched_keywords) / static_cast<float>(total_keywords);

    // Adjustment factor: at least 1 matched keyword is meaningful
    if (matched_keywords == 0) {
        return 0.0f;
    }

    // Add weight: more matches, higher confidence
    float weight = std::min(1.0f, static_cast<float>(matched_keywords) / 3.0f);

    return std::min(1.0f, base_confidence + weight * 0.5f);
}

auto TaskClassifier::generate_reasoning(
    TaskCategory category,
    const std::vector<std::string>& matched_keywords
) -> std::string {
    std::ostringstream oss;

    oss << "Detected keywords: [";
    for (size_t i = 0; i < matched_keywords.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << matched_keywords[i];
    }
    oss << "], classified as " << category_to_string(category);

    return oss.str();
}

} // namespace moltcat::agent
