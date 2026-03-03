#pragma once

#include "agent_template.hpp"
#include "model/agent_config.hpp"
#include <string>
#include <vector>

namespace moltcat::agent {

/**
 * @brief Agent template builder
 *
 * Provides fluent API for building AgentTemplate objects
 */
class AgentTemplateBuilder {
public:
    /**
     * @brief Set template ID
     */
    auto set_template_id(std::string_view id) -> AgentTemplateBuilder& {
        template_id_ = std::string(id);
        return *this;
    }

    /**
     * @brief Set template name
     */
    auto set_name(std::string_view name) -> AgentTemplateBuilder& {
        name_ = std::string(name);
        return *this;
    }

    /**
     * @brief Set description
     */
    auto set_description(std::string_view description) -> AgentTemplateBuilder& {
        description_ = std::string(description);
        return *this;
    }

    // ========== Provider Configuration ==========

    /**
     * @brief Set Provider ID
     */
    auto set_provider_id(std::string_view provider_id) -> AgentTemplateBuilder& {
        config_.provider_id = std::string(provider_id);
        return *this;
    }

    /**
     * @brief Set provider (maintain backward compatibility)
     */
    auto set_provider(std::string_view provider) -> AgentTemplateBuilder& {
        config_.provider_id = std::string(provider);
        return *this;
    }

    // ========== Resource Limits ==========

    /**
     * @brief Set maximum memory (bytes)
     */
    auto set_max_memory(uint64_t bytes) -> AgentTemplateBuilder& {
        config_.max_memory_bytes = bytes;
        return *this;
    }

    /**
     * @brief Set task timeout (milliseconds)
     */
    auto set_task_timeout(uint64_t timeout_ms) -> AgentTemplateBuilder& {
        config_.task_timeout_ms = timeout_ms;
        return *this;
    }

    // ========== Rate Limits ==========

    /**
     * @brief Set requests per minute limit
     */
    auto set_requests_per_minute(uint32_t rpm) -> AgentTemplateBuilder& {
        config_.requests_per_minute = rpm;
        return *this;
    }

    /**
     * @brief Set tokens per minute limit
     */
    auto set_tokens_per_minute(uint32_t tpm) -> AgentTemplateBuilder& {
        config_.tokens_per_minute = tpm;
        return *this;
    }

    // ========== Capability Configuration ==========

    /**
     * @brief Add capability
     */
    auto add_capability(std::string_view capability) -> AgentTemplateBuilder& {
        config_.allowed_capabilities.emplace_back(capability);
        return *this;
    }

    /**
     * @brief Set capability list
     */
    auto set_capabilities(const std::vector<std::string>& capabilities) -> AgentTemplateBuilder& {
        config_.allowed_capabilities = capabilities;
        return *this;
    }

    // ========== Metadata Configuration ==========

    /**
     * @brief Set version
     */
    auto set_version(std::string_view version) -> AgentTemplateBuilder& {
        metadata_.version = std::string(version);
        return *this;
    }

    /**
     * @brief Set author
     */
    auto set_author(std::string_view author) -> AgentTemplateBuilder& {
        metadata_.author = std::string(author);
        return *this;
    }

    /**
     * @brief Add tag
     */
    auto add_tag(std::string_view tag) -> AgentTemplateBuilder& {
        metadata_.tags.emplace_back(tag);
        return *this;
    }

    /**
     * @brief Set tag list
     */
    auto set_tags(const std::vector<std::string>& tags) -> AgentTemplateBuilder& {
        metadata_.tags = tags;
        return *this;
    }

    /**
     * @brief Add custom field
     */
    auto set_custom_field(std::string_view key, std::string_view value) -> AgentTemplateBuilder&& {
        metadata_.custom_fields[std::string(key)] = std::string(value);
        return std::move(*this);
    }

    // ========== Build Methods ==========

    /**
     * @brief Build AgentTemplate
     *
     * @return AgentTemplate object
     */
    [[nodiscard]] auto build() const -> AgentTemplate;

    /**
     * @brief Build and return smart pointer
     */
    [[nodiscard]] auto build_ptr() const -> std::shared_ptr<AgentTemplate>;

    /**
     * @brief Reset builder
     */
    auto reset() -> void;

private:
    std::string template_id_;
    std::string name_;
    std::string description_;
    model::AgentConfig config_;
    model::AgentMetadata metadata_;
};

} // namespace moltcat::agent
