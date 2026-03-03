#pragma once

#include "provider_config.hpp"
#include "../network/llm_client.hpp"
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <string>

namespace moltcat::provider {

/**
 * @brief Provider class (LLM client connection pool)
 *
 * Manages connection pool for LLM clients, multiple Agents can share the same Provider
 * Responsibilities:
 * - Maintain LLM client connection pool
 * - Provide interfaces for acquiring/releasing connections
 * - Manage connection lifecycle
 */
class Provider {
public:
    /**
     * @brief Constructor
     *
     * @param config Provider configuration
     */
    explicit Provider(const model::ProviderConfig& config);

    /**
     * @brief Destructor
     */
    ~Provider();

    // ========== Disable copy and move ==========
    Provider(const Provider&) = delete;
    Provider& operator=(const Provider&) = delete;
    Provider(Provider&&) = delete;
    Provider& operator=(Provider&&) = delete;

    // ========== Connection management ==========

    /**
     * @brief Acquire LLM client (from connection pool)
     *
     * @param model Model name (optional, use default model if empty)
     * @return LLM client pointer
     *
     * @note If connection pool is not full, create new connection; otherwise wait for available connection
     */
    [[nodiscard]] auto acquire_client(const std::string& model = "")
        -> network::ILlmClient*;

    /**
     * @brief Release LLM client (return to connection pool)
     *
     * @param client LLM client pointer
     */
    auto release_client(network::ILlmClient* client) -> void;

    // ========== Information queries ==========

    /**
     * @brief Get Provider ID
     */
    [[nodiscard]] auto get_provider_id() const -> const std::string& {
        return config_.provider_id;
    }

    /**
     * @brief Get Provider name
     */
    [[nodiscard]] auto get_provider_name() const -> const std::string& {
        return config_.provider_name;
    }

    /**
     * @brief Get configuration
     */
    [[nodiscard]] auto get_config() const -> const model::ProviderConfig& {
        return config_;
    }

    /**
     * @brief Get current active connection count
     */
    [[nodiscard]] auto get_active_connections() const -> uint32_t {
        return active_connections_.load();
    }

    /**
     * @brief Check if available (connection pool not full or has available connections)
     */
    [[nodiscard]] auto is_available() const -> bool;

private:
    model::ProviderConfig config_;               // Configuration

    // Connection pool
    std::vector<std::unique_ptr<network::ILlmClient>> clients_;
    mutable std::mutex pool_mutex_;               // Protect connection pool

    std::atomic<uint32_t> active_connections_{0};  // Current active connection count
    std::atomic<uint32_t> created_connections_{0}; // Total created connections

    /**
     * @brief Create new LLM client
     *
     * @return LLM client pointer
     */
    [[nodiscard]] auto create_client() -> std::unique_ptr<network::ILlmClient>;

    /**
     * @brief Check if need to create new connection
     */
    [[nodiscard]] auto should_create_client() const -> bool;
};

} // namespace moltcat::provider
