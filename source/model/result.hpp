#pragma once

#include "types.hpp"
#include <glaze/json.hpp>
#include <string>
#include <vector>
#include <map>

namespace moltcat::model {

/**
 * @brief Execution result
 *
 * Encapsulates task execution result data
 */
struct Result {
    // ========== Basic Information ==========
    std::string result_id;                          // Result ID
    std::string task_id;                            // Associated task ID
    std::string agent_id;                           // Executing agent ID
    ResultStatus status{ResultStatus::SUCCESS};      // Execution status

    // ========== Time Information ==========
    uint64_t created_at{0};                         // Creation timestamp
    uint64_t completed_at{0};                       // Completion timestamp
    uint64_t execution_duration_ms{0};              // Execution duration (milliseconds)

    // ========== Result Data ==========
    glz::json_t data;                              // Result data
    std::string output;                             // Text output (optional)

    // ========== Error Information ==========
    std::string error_code;                         // Error code
    std::string error_message;                      // Error message
    std::string error_stack;                        // Error stack

    // ========== Performance Metrics ==========
    uint64_t memory_used_bytes{0};                  // Memory usage
    uint32_t cpu_time_ms{0};                        // CPU time
    uint32_t io_operations{0};                      // IO operation count

    // ========== Quality Metrics ==========
    float confidence{0.0f};                         // Confidence [0.0, 1.0]
    float accuracy{0.0f};                           // Accuracy [0.0, 1.0]
    std::map<std::string, float> metrics;           // Custom metrics

    // ========== Intermediate Results ==========
    std::vector<glz::json_t> intermediate_results;  // Intermediate results (for debugging)
    std::vector<std::string> execution_logs;        // Execution logs

    // ========== Extended Fields ==========
    glz::json_t metadata;                           // Extended metadata
};

} // namespace moltcat::model

// Glaze serialization support - Result
template <>
struct glz::meta<moltcat::model::Result> {
    using T = moltcat::model::Result;
    static constexpr auto value = glz::object<
        "result_id",              &T::result_id,
        "task_id",                &T::task_id,
        "agent_id",                &T::agent_id,
        "status",                 &T::status,
        "created_at",              &T::created_at,
        "completed_at",            &T::completed_at,
        "execution_duration_ms",   &T::execution_duration_ms,
        "data",                    &T::data,
        "output",                  &T::output,
        "error_code",              &T::error_code,
        "error_message",           &T::error_message,
        "error_stack",             &T::error_stack,
        "memory_used_bytes",       &T::memory_used_bytes,
        "cpu_time_ms",             &T::cpu_time_ms,
        "io_operations",           &T::io_operations,
        "confidence",              &T::confidence,
        "accuracy",                &T::accuracy,
        "metrics",                 &T::metrics,
        "intermediate_results",    &T::intermediate_results,
        "execution_logs",          &T::execution_logs,
        "metadata",                &T::metadata
    >;
};
