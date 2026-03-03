#pragma once

#include "agent_template.hpp"
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <vector>
#include <string>

namespace moltcat::agent {

/**
 * @brief Agent template registry
 *
 * Manages all AgentTemplate, supports thread-safe registration and query
 * Singleton pattern, globally unique
 */
class AgentTemplateRegistry {
public:
    /**
     * @brief Get global singleton
     */
    [[nodiscard]] static auto get_instance() -> AgentTemplateRegistry&;

    // Disable copy and move
    AgentTemplateRegistry(const AgentTemplateRegistry&) = delete;
    AgentTemplateRegistry& operator=(const AgentTemplateRegistry&) = delete;
    AgentTemplateRegistry(AgentTemplateRegistry&&) = delete;
    AgentTemplateRegistry& operator=(AgentTemplateRegistry&&) = delete;

    /**
     * @brief Destructor
     */
    ~AgentTemplateRegistry() = default;

    // ========== Template Management ==========

    /**
     * @brief Register template
     *
     * @param templ Template smart pointer
     */
    auto register_template(std::shared_ptr<AgentTemplate> templ) -> void;

    /**
     * @brief Unregister template
     *
     * @param template_id Template ID
     * @return Success status
     */
    auto unregister_template(std::string_view template_id) -> bool;

    /**
     * @brief Get template
     *
     * @param template_id Template ID
     * @return Template smart pointer, returns nullptr if not found
     */
    [[nodiscard]] auto get_template(std::string_view template_id) const
        -> std::shared_ptr<AgentTemplate>;

    /**
     * @brief Check if template exists
     */
    [[nodiscard]] auto has_template(std::string_view template_id) const -> bool;

    /**
     * @brief Get all template IDs
     *
     * @return Template ID list
     */
    [[nodiscard]] auto list_templates() const -> std::vector<std::string>;

    /**
     * @brief Get template count
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    // ========== Load from Configuration ==========

    /**
     * @brief Load template from JSON configuration file
     *
     * @param config_path Configuration file path
     * @return Number of loaded templates
     */
    auto load_from_config(std::string_view config_path) -> size_t;

    /**
     * @brief Batch load template configurations from directory
     *
     * @param directory Configuration directory
     * @return Number of loaded templates
     */
    auto load_from_directory(std::string_view directory) -> size_t;

    /**
     * @brief Clear all templates
     */
    auto clear() noexcept -> void;

private:
    AgentTemplateRegistry() = default;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<AgentTemplate>> templates_;
};

// Global singleton access macro
#define AGENT_TEMPLATE_REGISTRY moltcat::agent::AgentTemplateRegistry::get_instance()

} // namespace moltcat::agent
