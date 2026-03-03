#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <vector>
#include <unordered_map>
#include "token_manager.hpp"
#include "../utils/error.hpp"

namespace moltcat::gateway::auth {

// Forward declarations
class PermissionChecker;

/**
 * @brief Authentication manager
 *
 * Responsibilities:
 * - Token verification
 * - Device signature verification
 * - Device registration management
 */
class AuthManager {
public:
    /**
     * @brief Constructor
     *
     * @param token_manager Token manager
     */
    explicit AuthManager(std::shared_ptr<token::TokenManager> token_manager);

    ~AuthManager() = default;

    // Disable copy
    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

    /**
     * @brief Verify Token
     *
     * @param token JWT Token
     * @return Result<token::TokenInfo> Token information, or error
     */
    [[nodiscard]] auto verify_token(std::string_view token)
        -> Result<token::TokenInfo>;

    /**
     * @brief Verify device signature (challenge-response)
     *
     * @param device_id Device ID
     * @param public_key Base64-encoded public key
     * @param signature Base64-encoded signature
     * @param challenge_payload Challenge payload
     * @return Result<bool> true indicates verification passed, or error
     */
    [[nodiscard]] auto verify_device_signature(
        std::string_view device_id,
        std::string_view public_key,
        std::string_view signature,
        std::string_view challenge_payload
    ) -> Result<bool>;

    /**
     * @brief Issue device Token
     *
     * @param device_id Device ID
     * @param role Role
     * @param scopes Permission scopes
     * @param ttl_seconds Validity period
     * @return Result<std::string> JWT Token
     */
    [[nodiscard]] auto issue_device_token(
        std::string_view device_id,
        std::string_view role,
        const std::vector<std::string>& scopes = {},
        uint64_t ttl_seconds = 0
    ) -> Result<std::string>;

    /**
     * @brief Rotate device Token
     *
     * @param old_token Old Token
     * @param ttl_seconds New Token validity period
     * @return Result<std::string> New Token
     */
    [[nodiscard]] auto rotate_device_token(
        std::string_view old_token,
        uint64_t ttl_seconds = 0
    ) -> Result<std::string>;

    /**
     * @brief Revoke device Token
     *
     * @param token Token
     * @return Result<void>
     */
    auto revoke_device_token(std::string_view token) -> Result<void>;

    /**
     * @brief Register device (store public key)
     *
     * @param device_id Device ID
     * @param public_key Base64-encoded public key
     * @return Result<void>
     */
    auto register_device(std::string_view device_id, std::string_view public_key)
        -> Result<void>;

    /**
     * @brief Unregister device
     *
     * @param device_id Device ID
     * @return Result<void>
     */
    auto unregister_device(std::string_view device_id) -> Result<void>;

    /**
     * @brief Get device public key
     *
     * @param device_id Device ID
     * @return std::optional<std::string> Base64-encoded public key, returns nullopt if not found
     */
    [[nodiscard]] auto get_device_public_key(std::string_view device_id) const
        -> std::optional<std::string>;

    /**
     * @brief Check if device is registered
     *
     * @param device_id Device ID
     * @return bool Whether registered
     */
    [[nodiscard]] auto is_device_registered(std::string_view device_id) const -> bool;

    /**
     * @brief Validate device ID format
     *
     * @param device_id Device ID
     * @return bool Whether valid format
     */
    [[nodiscard]] static auto validate_device_id(std::string_view device_id) -> bool;

    /**
     * @brief Derive device ID from public key
     *
     * @param public_key Base64-encoded public key
     * @return Result<std::string> Device ID
     */
    [[nodiscard]] static auto derive_device_id(std::string_view public_key)
        -> Result<std::string>;

private:
    std::shared_ptr<token::TokenManager> token_manager_;

    // Device registry (device_id -> public_key)
    std::unordered_map<std::string, std::string> device_registry_;
    mutable std::shared_mutex registry_mutex_;
};

} // namespace moltcat::gateway::auth
