#pragma once

#include "provider.hpp"
#include "model/provider_config.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <shared_mutex>

namespace moltcat::provider {

/**
 * @brief Provider manager (singleton)
 *
 * Global singleton, manages all Provider instances
 * Supports loading Providers from configuration files
 */
class ProviderManager {
public:
    /**
     * @brief Get singleton instance
     *
     * @return ProviderManager reference
     */
    [[nodiscard]] static auto get_instance() -> ProviderManager&;

    // ========== Disable copy and move ==========
    ProviderManager(const ProviderManager&) = delete;
    ProviderManager& operator=(const ProviderManager&) = delete;
    ProviderManager(ProviderManager&&) = delete;
    ProviderManager& operator=(ProviderManager&&) = delete;

    // ========== Provider management ==========

    /**
     * @brief Register Provider
     *
     * @param provider Provider instance
     * @return true Success, false Failed (ID conflict)
     */
    auto register_provider(std::unique_ptr<Provider> provider) -> bool;

    /**
     * @brief Create and register Provider from configuration
     *
     * @param config Provider configuration
     * @return true Success, false Failed
     */
    [[nodiscard]] auto create_provider(const model::ProviderConfig& config)
        -> bool;

    /**
     * @brief Unregister Provider
     *
     * @param provider_id Provider ID
     * @return true Success, false Failed (not exists)
     */
    auto unregister_provider(std::string_view provider_id) -> bool;

    /**
     * @brief Get Provider
     *
     * @param provider_id Provider ID
     * @return Provider pointer, nullptr on failure
     */
    [[nodiscard]] auto get_provider(std::string_view provider_id) const
        -> Provider*;

    /**
     * @brief Get Provider (const version)
     */
    [[nodiscard]] auto get_provider(std::string_view provider_id) const
        -> const Provider*;

    /**
     * @brief Check if Provider exists
     *
     * @param provider_id Provider ID
     * @return true Exists, false Does not exist
     */
    [[nodiscard]] auto has_provider(std::string_view provider_id) const -> bool;

    /**
     * @brief Get all Provider ID list
     *
     * @return Provider ID list
     */
    [[nodiscard]] auto list_providers() const -> std::vector<std::string>;

    /**
     * @brief Load Provider configuration from JSON file
     *
     * @param file_path Configuration file path
     * @return Number of loaded Providers
     */
    auto load_from_file(std::string_view file_path) -> size_t;

    /**
     * @brief Get default Provider
     *
     * @return Provider pointer, nullptr on failure
     */
    [[nodiscard]] auto get_default_provider() const -> Provider*;

    /**
     * @brief Set default Provider ID
     *
     * @param provider_id Provider ID
     */
    auto set_default_provider(std::string_view provider_id) -> void;

private:
    /**
     * @brief Constructor (private)
     */
    ProviderManager() = default;

    /**
     * @brief Destructor (private)
     */
    ~ProviderManager();

    // Provider storage
    std::unordered_map<std::string, std::unique_ptr<Provider>> providers_;

    mutable std::shared_mutex mutex_;  // Read-write lock protection

    std::string default_provider_id_;  // Default Provider ID
};

// Global singleton access macro
#define PROVIDER_MANAGER moltcat::provider::ProviderManager::get_instance()

} // namespace moltcat::provider
