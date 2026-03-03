#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include <optional>
#include <memory>
#include "../utils/error.hpp"

namespace moltcat::gateway::token {

/**
 * @brief Token information
 */
struct TokenInfo {
    std::string device_id;
    std::string role;                    // operator | node
    std::vector<std::string> scopes;     // operator.read, operator.write...
    std::vector<std::string> caps;       // code-generation, code-review...
    std::vector<std::string> commands;   // task.execute, task.cancel...
    std::map<std::string, bool> permissions;  // Fine-grained permissions

    uint64_t issued_at;      // Issuance time (Unix timestamp, seconds)
    uint64_t expires_at;     // Expiration time (Unix timestamp, seconds)
};

/**
 * @brief Token payload (JWT Payload)
 */
struct TokenPayload {
    std::string device_id;
    std::string role;
    std::vector<std::string> scopes;
    std::vector<std::string> caps;
    std::vector<std::string> commands;
    std::map<std::string, bool> permissions;

    uint64_t iat;  // Issued At
    uint64_t exp;  // Expiration

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] auto to_json() const -> std::string;

    /**
     * @brief Parse from JSON
     */
    static auto from_json(std::string_view json) -> Result<TokenPayload>;

    /**
     * @brief Convert to TokenInfo
     */
    [[nodiscard]] auto to_info() const -> TokenInfo;
};

/**
 * @brief JWT Token manager
 *
 * Responsible for issuing, verifying, and managing JWT Tokens
 * Uses EdDSA (Ed25519) signature algorithm
 */
class TokenManager {
public:
    /**
     * @brief Token manager configuration
     */
    struct Config {
        std::string issuer = "moltcat-gateway";  // Issuer
        uint64_t default_ttl_seconds = 86400;    // Default validity period (24 hours)
        uint64_t max_ttl_seconds = 604800;       // Maximum validity period (7 days)
    };

    /**
     * @brief Constructor
     *
     * @param config Configuration
     */
    explicit TokenManager(const Config& config);

    /**
     * @brief Destructor (securely erase private key)
     */
    ~TokenManager();

    // Disable copy
    TokenManager(const TokenManager&) = delete;
    TokenManager& operator=(const TokenManager&) = delete;

    // Allow move
    TokenManager(TokenManager&&) noexcept;
    TokenManager& operator=(TokenManager&&) noexcept;

    /**
     * @brief Issue device Token
     *
     * @param device_id Device ID
     * @param role Role (operator | node)
     * @param scopes Permission scopes
     * @param caps Capability categories (node specific)
     * @param commands Command whitelist (node specific)
     * @param permissions Fine-grained permissions (node specific)
     * @param ttl_seconds Validity period (seconds), 0 for default
     * @return Result<std::string> JWT Token string, or error
     */
    [[nodiscard]] auto issue_device_token(
        std::string_view device_id,
        std::string_view role,
        const std::vector<std::string>& scopes = {},
        const std::vector<std::string>& caps = {},
        const std::vector<std::string>& commands = {},
        const std::map<std::string, bool>& permissions = {},
        uint64_t ttl_seconds = 0
    ) -> Result<std::string>;

    /**
     * @brief Verify Token
     *
     * @param token JWT Token string
     * @return Result<TokenInfo> Token information, or error
     */
    [[nodiscard]] auto verify_token(std::string_view token) -> Result<TokenInfo>;

    /**
     * @brief Rotate device Token
     *
     * @param old_token Old Token
     * @param ttl_seconds New Token validity period (seconds), 0 for default
     * @return Result<std::string> New Token, or error
     */
    [[nodiscard]] auto rotate_device_token(
        std::string_view old_token,
        uint64_t ttl_seconds = 0
    ) -> Result<std::string>;

    /**
     * @brief Revoke Token (add to blacklist)
     *
     * @param token Token to revoke
     * @return Result<void>
     */
    auto revoke_token(std::string_view token) -> Result<void>;

    /**
     * @brief Check if Token is revoked
     *
     * @param token Token string or Token ID
     * @return bool Whether revoked
     */
    [[nodiscard]] auto is_revoked(std::string_view token) const -> bool;

    /**
     * @brief Get public key (for verification)
     *
     * @return std::string Base64-encoded public key
     */
    [[nodiscard]] auto get_public_key() const -> std::string;

    /**
     * @brief Set public key (for verifying external Tokens)
     *
     * @param public_key_base64 Base64-encoded public key
     * @return Result<void>
     */
    auto set_public_key(std::string_view public_key_base64) -> Result<void>;

private:
    Config config_;

    // Ed25519 key pair (for signing)
    std::vector<std::byte> private_key_;
    std::vector<std::byte> public_key_;
    std::string public_key_base64_;

    // Token blacklist (store Token ID or hash of complete Token)
    std::unordered_set<std::string> revoked_tokens_;
    mutable std::shared_mutex revoked_mutex_;

    // Internal helper methods

    /**
     * @brief Generate JWT Token
     */
    [[nodiscard]] auto generate_jwt(
        const TokenPayload& payload
    ) const -> Result<std::string>;

    /**
     * @brief Parse JWT Token
     */
    [[nodiscard]] auto parse_jwt(
        std::string_view token
    ) const -> Result<TokenPayload>;

    /**
     * @brief Verify JWT signature
     */
    [[nodiscard]] auto verify_jwt_signature(
        std::string_view header_b64,
        std::string_view payload_b64,
        std::string_view signature
    ) const -> bool;

    /**
     * @brief Generate Token ID (for blacklist)
     */
    [[nodiscard]] static auto extract_token_id(std::string_view token) -> std::string;

    /**
     * @brief Base64 URL encode
     */
    [[nodiscard]] static auto base64_url_encode(std::string_view data) -> std::string;

    /**
     * @brief Base64 URL decode
     */
    [[nodiscard]] static auto base64_url_decode(std::string_view data) -> Result<std::string>;
};

/**
 * @brief Token verification error codes
 */
enum class TokenError {
    INVALID_FORMAT = 1001,      // Invalid format
    INVALID_SIGNATURE = 1002,   // Invalid signature
    EXPIRED = 1003,             // Expired
    REVOKED = 1004,             // Revoked
    ISSUER_MISMATCH = 1005,     // Issuer mismatch
};

} // namespace moltcat::gateway::token
