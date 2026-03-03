#include "agent_template_builder.hpp"
#include "utils/uuid.hpp"

namespace moltcat::agent {

auto AgentTemplateBuilder::build() const -> AgentTemplate {
    // Validate required fields
    if (template_id_.empty()) {
        // TODO: Add error handling
        throw std::invalid_argument("template_id cannot be empty");
    }

    if (name_.empty()) {
        name_ = template_id_;  // Use template_id as default name
    }

    return AgentTemplate(template_id_, name_, description_, config_, metadata_);
}

auto AgentTemplateBuilder::build_ptr() const -> std::shared_ptr<AgentTemplate> {
    return std::make_shared<AgentTemplate>(build());
}

auto AgentTemplateBuilder::reset() -> void {
    template_id_.clear();
    name_.clear();
    description_.clear();
    config_ = model::AgentConfig{};
    metadata_ = model::AgentMetadata{};
}

} // namespace moltcat::agent
