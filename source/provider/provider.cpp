#include "provider.hpp"
#include "../network/llm_client_factory.hpp"
#include "../utils/logger.hpp"
#include <format>
#include <algorithm>
#include <condition_variable>

namespace moltcat::provider {

Provider::Provider(const model::ProviderConfig& config)
    : config_(config)
    , clients_()
    , pool_mutex_()
    , active_connections_(0)
    , created_connections_(0)
{
    clients_.reserve(config_.max_connections);
    MOLT_LOGGER.info("Created Provider: id={}, name={}, max_connections={}",
                     config_.provider_id,
                     config_.provider_name,
                     config_.max_connections);
}

Provider::~Provider() {
    MOLT_LOGGER.info("Destroying Provider: id={}, active_connections={}",
                     config_.provider_id,
                     active_connections_.load());

    // Wait for all connections to be returned
    while (active_connections_.load() > 0) {
        MOLT_LOGGER.debug("Waiting for connection return: {} remaining",
                          active_connections_.load());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Clean up all clients
    std::lock_guard lock(pool_mutex_);
    clients_.clear();
}

auto Provider::acquire_client(const std::string& model)
    -> network::ILlmClient* {
    // First try to find idle connection
    {
        std::lock_guard lock(pool_mutex_);

        // Find idle connection (connection not actively used)
        for (auto& client : clients_) {
            // Simple strategy: if client exists and not marked active, reuse
            // Actual implementation needs more complex connection availability check
            // Simplified here: round-robin allocation
            if (client && active_connections_.load() < clients_.size()) {
                active_connections_.fetch_add(1);
                MOLT_LOGGER.debug("Provider {} reused connection: active={}/{}",
                                  config_.provider_id,
                                  active_connections_.load(),
                                  clients_.size());
                return client.get();
            }
        }

        // Check if can create new connection
        if (should_create_client()) {
            auto new_client = create_client();
            if (new_client) {
                active_connections_.fetch_add(1);
                clients_.push_back(std::move(new_client));
                MOLT_LOGGER.info("Provider {} created new connection: active={}/total={}",
                                 config_.provider_id,
                                 active_connections_.load(),
                                 clients_.size());
                return clients_.back().get();
            }
        }

        // Connection pool full, need to wait
        MOLT_LOGGER.warn("Provider {} connection pool full, waiting for available connection",
                          config_.provider_id);
    }

    // Wait for available connection (simplified: short wait then return nullptr)
    // Should use condition variable to wait
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Retry search
    {
        std::lock_guard lock(pool_mutex_);
        for (auto& client : clients_) {
            if (client && active_connections_.load() < clients_.size()) {
                active_connections_.fetch_add(1);
                return client.get();
            }
        }
    }

    MOLT_LOGGER.error("Provider {} unable to acquire connection",
                     config_.provider_id);
    return nullptr;
}

auto Provider::release_client(network::ILlmClient* client) -> void {
    if (!client) {
        MOLT_LOGGER.warn("Attempting to release null connection pointer");
        return;
    }

    std::lock_guard lock(pool_mutex_);

    // Find corresponding client and mark as idle
    // Simplified: only decrement active connection count
    auto prev_count = active_connections_.fetch_sub(1);

    MOLT_LOGGER.debug("Provider {} released connection: active={}/{}",
                      config_.provider_id,
                      active_connections_.load(),
                      clients_.size());

    if (prev_count == 0) {
        MOLT_LOGGER.error("Provider {} active connection count abnormal",
                         config_.provider_id);
    }
}

auto Provider::create_client() -> std::unique_ptr<network::ILlmClient> {
    // Use factory to create client
    // Need temporary AgentConfig to call factory
    model::AgentConfig temp_config;
    temp_config.provider = config_.provider_name;
    temp_config.api_key = config_.api_key;
    temp_config.api_endpoint = config_.api_endpoint;
    temp_config.temperature = config_.default_temperature;
    temp_config.max_tokens = config_.default_max_tokens;
    temp_config.connection_timeout_ms = config_.connection_timeout_ms;

    auto client = network::LlmClientFactory::create(temp_config);

    if (client) {
        created_connections_.fetch_add(1);
        MOLT_LOGGER.info("Provider {} created new client successfully: total={}",
                         config_.provider_id,
                         created_connections_.load());
    } else {
        MOLT_LOGGER.error("Provider {} failed to create client",
                         config_.provider_id);
    }

    return client;
}

auto Provider::should_create_client() const -> bool {
    auto current = clients_.size();
    auto max = config_.max_connections;

    // Can create more connections
    return current < max;
}

auto Provider::is_available() const -> bool {
    // Check if has available connections
    auto active = active_connections_.load();
    auto total = clients_.size();

    return active < total || total < config_.max_connections;
}

} // namespace moltcat::provider
