#include "provider_manager.hpp"
#include "../utils/logger.hpp"
#include "../utils/json_utils.hpp"
#include <filesystem>
#include <format>

namespace moltcat::provider {

ProviderManager::ProviderManager()
    : providers_()
    , mutex_()
    , default_provider_id_()
{
    MOLT_LOGGER.info("Created ProviderManager singleton");
}

ProviderManager::~ProviderManager() {
    MOLT_LOGGER.info("Destroying ProviderManager singleton");
}

auto ProviderManager::get_instance() -> ProviderManager& {
    static ProviderManager instance;
    return instance;
}

auto ProviderManager::register_provider(std::unique_ptr<Provider> provider)
    -> bool {
    if (!provider) {
        MOLT_LOGGER.error("Failed to register Provider: Provider is null");
        return false;
    }

    auto provider_id = provider->get_provider_id();

    std::unique_lock lock(mutex_);

    // Check ID conflict
    if (providers_.find(provider_id) != providers_.end()) {
        MOLT_LOGGER.warn("Provider already exists: {}", provider_id);
        return false;
    }

    providers_[provider_id] = std::move(provider);
    MOLT_LOGGER.info("Registered Provider: id={}, name={}, total={}",
                     provider_id,
                     providers_[provider_id]->get_provider_name(),
                     providers_.size());

    // If first Provider, set as default
    if (default_provider_id_.empty()) {
        default_provider_id_ = provider_id;
        MOLT_LOGGER.info("Set default Provider: {}", provider_id);
    }

    return true;
}

auto ProviderManager::create_provider(const model::ProviderConfig& config)
    -> bool {
    auto provider = std::make_unique<Provider>(config);
    return register_provider(std::move(provider));
}

auto ProviderManager::unregister_provider(std::string_view provider_id)
    -> bool {
    std::unique_lock lock(mutex_);

    auto it = providers_.find(std::string(provider_id));
    if (it == providers_.end()) {
        MOLT_LOGGER.warn("Provider does not exist: {}", provider_id);
        return false;
    }

    // Clean up Provider (wait for all connections to return)
    it->second.reset();

    providers_.erase(it);

    // If deleted default Provider, clear default setting
    if (default_provider_id_ == provider_id) {
        default_provider_id_.clear();
        if (!providers_.empty()) {
            default_provider_id_ = providers_.begin()->first;
            MOLT_LOGGER.info("Updated default Provider: {}", default_provider_id_);
        }
    }

    MOLT_LOGGER.info("Unregistered Provider: id={}, remaining={}",
                     provider_id, providers_.size());

    return true;
}

auto ProviderManager::get_provider(std::string_view provider_id) const
    -> Provider* {
    std::shared_lock lock(mutex_);

    auto it = providers_.find(std::string(provider_id));
    if (it != providers_.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto ProviderManager::get_provider(std::string_view provider_id) const
    -> const Provider* {
    std::shared_lock lock(mutex_);

    auto it = providers_.find(std::string(provider_id));
    if (it != providers_.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto ProviderManager::has_provider(std::string_view provider_id) const
    -> bool {
    std::shared_lock lock(mutex_);
    return providers_.find(std::string(provider_id)) != providers_.end();
}

auto ProviderManager::list_providers() const -> std::vector<std::string> {
    std::vector<std::string> provider_ids;
    provider_ids.reserve(providers_.size());

    std::shared_lock lock(mutex_);
    for (const auto& [id, _] : providers_) {
        provider_ids.push_back(id);
    }

    return provider_ids;
}

auto ProviderManager::load_from_file(std::string_view file_path)
    -> size_t {
    MOLT_LOGGER.info("Loading Provider configuration from file: {}", file_path);

    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        MOLT_LOGGER.error("Configuration file does not exist: {}", file_path);
        return 0;
    }

    // Read JSON file
    auto json_result = utils::JsonUtils::load_from_file(std::string(file_path));
    if (!json_result) {
        MOLT_LOGGER.error("Failed to parse configuration file: {}",
                         json_result.error().message);
        return 0;
    }

    const auto& json = *json_result;

    // Support two formats:
    // 1. Single Provider configuration object
    // 2. Provider array
    std::vector<model::ProviderConfig> configs;

    if (json.holds<glz::json::object_t>()) {
        // Single configuration object
        auto config = json.get<model::ProviderConfig>();
        configs.push_back(config);
        MOLT_LOGGER.info("Loaded single Provider configuration: {}", config.provider_id);
    } else if (json.holds<glz::json::array_t>()) {
        // Configuration array
        const auto& configs_array = json.get<glz::json::array_t>();
        for (const auto& config_json : configs_array) {
            if (config_json.holds<model::ProviderConfig>()) {
                configs.push_back(config_json.get<model::ProviderConfig>());
            }
        }
        MOLT_LOGGER.info("Loaded Provider array: {} configurations", configs.size());
    } else {
        MOLT_LOGGER.error("Configuration file format error: must be object or array");
        return 0;
    }

    // Create and register all Providers
    size_t success_count = 0;
    for (const auto& config : configs) {
        if (create_provider(config)) {
            success_count++;
        }
    }

    MOLT_LOGGER.info("Provider loading complete: {}/{} successful",
                     success_count, configs.size());

    return success_count;
}

auto ProviderManager::get_default_provider() const -> Provider* {
    if (default_provider_id_.empty()) {
        return nullptr;
    }

    return get_provider(default_provider_id_);
}

auto ProviderManager::set_default_provider(std::string_view provider_id)
    -> void {
    std::unique_lock lock(mutex_);

    if (!has_provider(provider_id)) {
        MOLT_LOGGER.error("Failed to set default Provider: Provider does not exist: {}",
                         provider_id);
        return;
    }

    default_provider_id_ = provider_id;
    MOLT_LOGGER.info("Set default Provider: {}", provider_id_);
}

} // namespace moltcat::provider
