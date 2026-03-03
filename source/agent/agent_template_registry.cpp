#include "agent_template_registry.hpp"
#include "utils/logger.hpp"
#include "utils/file_utils.hpp"
#include "utils/json_utils.hpp"
#include "utils/string_utils.hpp"
#include <glaze/glaze.hpp>
#include <algorithm>

namespace moltcat::agent {

auto AgentTemplateRegistry::get_instance() -> AgentTemplateRegistry& {
    static AgentTemplateRegistry instance;
    return instance;
}

auto AgentTemplateRegistry::register_template(std::shared_ptr<AgentTemplate> templ)
    -> void {
    if (!templ) {
        MOLT_LOGGER.error("Attempted to register null template");
        return;
    }

    const auto& template_id = templ->get_template_id();
    if (template_id.empty()) {
        MOLT_LOGGER.error("Template ID cannot be empty");
        return;
    }

    std::unique_lock lock(mutex_);
    templates_[template_id] = templ;

    MOLT_LOGGER.info("Registered agent template: {}", template_id);
}

auto AgentTemplateRegistry::unregister_template(std::string_view template_id) -> bool {
    std::unique_lock lock(mutex_);
    auto it = templates_.find(template_id);

    if (it == templates_.end()) {
        MOLT_LOGGER.warn("Template not found: {}", template_id);
        return false;
    }

    templates_.erase(it);
    MOLT_LOGGER.info("Unregistered agent template: {}", template_id);
    return true;
}

auto AgentTemplateRegistry::get_template(std::string_view template_id) const
    -> std::shared_ptr<AgentTemplate> {
    std::shared_lock lock(mutex_);
    auto it = templates_.find(template_id);

    if (it != templates_.end()) {
        return it->second;
    }

    MOLT_LOGGER.warn("Template not found: {}", template_id);
    return nullptr;
}

auto AgentTemplateRegistry::has_template(std::string_view template_id) const -> bool {
    std::shared_lock lock(mutex_);
    return templates_.find(template_id) != templates_.end();
}

auto AgentTemplateRegistry::list_templates() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    std::vector<std::string> template_ids;

    template_ids.reserve(templates_.size());
    for (const auto& [id, templ] : templates_) {
        template_ids.push_back(id);
    }

    std::sort(template_ids.begin(), template_ids.end());
    return template_ids;
}

auto AgentTemplateRegistry::size() const noexcept -> size_t {
    std::shared_lock lock(mutex_);
    return templates_.size();
}

auto AgentTemplateRegistry::load_from_config(std::string_view config_path)
    -> size_t {
    // Read JSON file
    auto json_str = utils::FileUtils::read_text_file(config_path);
    if (!json_str) {
        MOLT_LOGGER.error("Failed to read config file: {}", config_path);
        return 0;
    }

    // Parse JSON
    glz::json_t json;
    auto err = glz::read_json(json_str.value(), json);
    if (err) {
        MOLT_LOGGER.error("Failed to parse config JSON: {}", config_path);
        return 0;
    }

    size_t loaded_count = 0;

    // Load templates array
    if (json.contains("templates") && json["templates"].is_array()) {
        for (const auto& templ_json : json["templates"]) {
            try {
                // Extract configuration
                model::AgentConfig config;
                auto config_err = glz::read_json(templ_json["config"], config);
                if (config_err) {
                    MOLT_LOGGER.warn("Failed to parse config for template: {}", templ_json);
                    continue;
                }

                // Extract metadata
                model::AgentMetadata metadata;
                auto metadata_err = glz::read_json(templ_json["metadata"], metadata);
                if (metadata_err) {
                    MOLT_LOGGER.warn("Failed to parse metadata for template: {}", templ_json);
                    continue;
                }

                // Create template
                auto templ = std::make_shared<AgentTemplate>(
                    templ_json["template_id"].get<std::string>(),
                    templ_json["name"].get<std::string>(),
                    templ_json["description"].get<std::string>(),
                    config,
                    metadata
                );

                // Register template
                register_template(templ);
                loaded_count++;

            } catch (const std::exception& e) {
                MOLT_LOGGER.error("Failed to load template: {}", e.what());
            }
        }
    }

    MOLT_LOGGER.info("Loaded {} agent templates from {}", loaded_count, config_path);
    return loaded_count;
}

auto AgentTemplateRegistry::load_from_directory(std::string_view directory) -> size_t {
    // List all .json files in directory
    auto files = utils::FileUtils::list_directory(directory);
    if (!files) {
        MOLT_LOGGER.error("Failed to list directory: {}", directory);
        return 0;
    }

    size_t total_loaded = 0;

    // Filter .json files
    std::vector<std::string> json_files;
    for (const auto& file : files.value()) {
        if (file.size() > 5 && file.substr(file.size() - 5) == ".json") {
            json_files.push_back(file);
        }
    }

    // Load each JSON file
    for (const auto& json_file : json_files) {
        std::string full_path = std::string(directory) + "/" + json_file;
        total_loaded += load_from_config(full_path);
    }

    MOLT_LOGGER.info("Loaded {} agent templates from directory: {}", total_loaded, directory);
    return total_loaded;
}

auto AgentTemplateRegistry::clear() noexcept -> void {
    std::unique_lock lock(mutex_);
    auto count = templates_.size();
    templates_.clear();

    MOLT_LOGGER.info("Cleared all agent templates ({} templates removed)", count);
}

} // namespace moltcat::agent
