#pragma once

#include "model/agent_config.hpp"
#include <string>
#include <vector>
#include <memory>

namespace moltcat::agent {

/**
 * @brief Agent template forward declaration
 */
class Agent;

/**
 * @brief Agent template
 *
 * Encapsulates static configuration of Agent, serves as blueprint for creating Agent instances
 *
 * Design principles:
 * - Lightweight: multiple Agent instances share the same template, saving memory
 * - Consistency: instances created from the same template have consistent behavior
 * - Extensible: supports configuration overrides to meet customization needs
 */
class AgentTemplate {
public:
    /**
     * @brief Constructor
     *
     * @param template_id Template ID (e.g., "code_assistant")
     * @param name Template name
     * @param description Template description
     * @param config Static configuration
     * @param metadata Metadata
     */
    AgentTemplate(
        std::string template_id,
        std::string name,
        std::string description,
        const model::AgentConfig& config,
        const model::AgentMetadata& metadata
    );

    // ========== Disable copy and move ==========
    AgentTemplate(const AgentTemplate&) = delete;
    AgentTemplate& operator=(const AgentTemplate&) = delete;
    AgentTemplate(AgentTemplate&&) = delete;
    AgentTemplate& operator=(AgentTemplate&&) = delete;

    // ========== Destructor ==========
    ~AgentTemplate() = default;

    // ========== Accessors ==========

    /**
     * @brief Get template ID
     */
    [[nodiscard]] auto get_template_id() const noexcept -> const std::string& {
        return template_id_;
    }

    /**
     * @brief Get template name
     */
    [[nodiscard]] auto get_name() const noexcept -> const std::string& {
        return name_;
    }

    /**
     * @brief Get template description
     */
    [[nodiscard]] auto get_description() const noexcept -> const std::string& {
        return description_;
    }

    /**
     * @brief Get static configuration
     */
    [[nodiscard]] auto get_config() const noexcept -> const model::AgentConfig& {
        return config_;
    }

    /**
     * @brief Get metadata
     */
    [[nodiscard]] auto get_metadata() const noexcept -> const model::AgentMetadata& {
        return metadata_;
    }

    /**
     * @brief Get capabilities list
     */
    [[nodiscard]] auto get_capabilities() const noexcept -> const std::vector<std::string>& {
        return config_.allowed_capabilities;
    }

    // ========== Factory methods ==========

    /**
     * @brief Create Agent instance
     *
     * @param instance_id Instance ID (e.g., "agent-001")
     * @return Smart pointer to Agent instance
     */
    [[nodiscard]] auto create_instance(std::string instance_id) const
        -> std::unique_ptr<Agent>;

    /**
     * @brief Create Agent instance (with configuration overrides)
     *
     * @param instance_id Instance ID
     * @param config_overrides Configuration overrides
     * @return Smart pointer to Agent instance
     */
    [[nodiscard]] auto create_instance(
        std::string instance_id,
        const model::AgentConfig& config_overrides
    ) const -> std::unique_ptr<Agent>;

    // ========== Configuration validation ==========

    /**
     * @brief Validate template configuration
     */
    [[nodiscard]] auto validate() const -> bool;

private:
    std::string template_id_;              // Template ID
    std::string name_;                     // Template name
    std::string description_;              // Template description
    const model::AgentConfig config_;     // Static configuration (reference)
    const model::AgentMetadata metadata_; // Metadata (reference)
};

} // namespace moltcat::agent
