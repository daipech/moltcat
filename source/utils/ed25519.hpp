#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include "error.hpp"

namespace moltcat::utils {

/**
 * @brief Ed25519 signature utility class
 *
 * Provides complete Ed25519 signature algorithm support, including:
 * - Key pair generation
 * - Message signing
 * - Signature verification
 * - Device fingerprint derivation (SHA256 hash based on public key)
 */
class Ed25519 {
public:
    // ==================== Constant definitions ====================

    /// Public key length (32 bytes)
    static constexpr size_t PUBLIC_KEY_SIZE = 32;

    /// Private key length (64 bytes, seed + public key)
    static constexpr size_t PRIVATE_KEY_SIZE = 64;

    /// Seed length (32 bytes)
    static constexpr size_t SEED_SIZE = 32;

    /// Signature length (64 bytes)
    static constexpr size_t SIGNATURE_SIZE = 64;

    // ==================== Data structures ====================

    /**
     * @brief Key pair
     */
    struct KeyPair {
        std::vector<std::byte> private_key;  // 64 bytes
        std::vector<std::byte> public_key;   // 32 bytes

        KeyPair() = default;

        KeyPair(std::vector<std::byte>&& priv, std::vector<std::byte>&& pub)
            : private_key(std::move(priv))
            , public_key(std::move(pub)) {}

        // Move constructor and assignment
        KeyPair(KeyPair&&) noexcept = default;
        KeyPair& operator=(KeyPair&&) noexcept = default;

        // Disable copy (for security)
        KeyPair(const KeyPair&) = delete;
        KeyPair& operator=(const KeyPair&) = delete;

        // Securely wipe private key
        ~KeyPair();
    };

    /**
     * @brief Signature
     */
    struct Signature {
        std::vector<std::byte> data;  // 64 bytes

        Signature() = default;
        explicit Signature(std::vector<std::byte>&& sig) : data(std::move(sig)) {}

        // Convert to Base64 string
        [[nodiscard]] auto to_base64() const -> std::string;

        // Parse from Base64 string
        static auto from_base64(std::string_view base64) -> Result<Signature>;

        // Validate signature length
        [[nodiscard]] auto is_valid() const -> bool {
            return data.size() == SIGNATURE_SIZE;
        }
    };

    // ==================== Key generation ====================

    /**
     * @brief Generate new Ed25519 key pair
     *
     * Uses libsodium's cryptographically secure random number generator
     *
     * @return Result<KeyPair> Generated key pair, or error
     */
    [[nodiscard]] static auto generate_keypair() -> Result<KeyPair>;

    /**
     * @brief Generate key pair from seed
     *
     * @param seed 32-byte seed
     * @return Result<KeyPair> Generated key pair, or error
     */
    [[nodiscard]] static auto keypair_from_seed(std::string_view seed) -> Result<KeyPair>;

    /**
     * @brief Extract public key from private key
     *
     * @param private_key 64-byte private key
     * @return Result<std::vector<std::byte>> 32-byte public key, or error
     */
    [[nodiscard]] static auto extract_public_key(
        std::string_view private_key
    ) -> Result<std::vector<std::byte>>;

    // ==================== Signing ====================

    /**
     * @brief Sign message with private key
     *
     * @param message Message to sign
     * @param private_key 64-byte private key
     * @return Signature 64-byte signature
     */
    [[nodiscard]] static auto sign(
        std::string_view message,
        std::string_view private_key
    ) -> Result<Signature>;

    /**
     * @brief Sign message with private key (byte array version)
     *
     * @param data Message data
     * @param len Data length
     * @param private_key 64-byte private key
     * @return Signature 64-byte signature
     */
    [[nodiscard]] static auto sign(
        const std::byte* data,
        size_t len,
        std::string_view private_key
    ) -> Result<Signature>;

    // ==================== Verification ====================

    /**
     * @brief Verify signature
     *
     * @param message Original message
     * @param signature Signature
     * @param public_key 32-byte public key
     * @return bool Whether signature is valid
     */
    [[nodiscard]] static auto verify(
        std::string_view message,
        const Signature& signature,
        std::string_view public_key
    ) -> bool;

    /**
     * @brief Verify signature (byte array version)
     *
     * @param data Message data
     * @param len Data length
     * @param signature Signature
     * @param public_key 32-byte public key
     * @return bool Whether signature is valid
     */
    [[nodiscard]] static auto verify(
        const std::byte* data,
        size_t len,
        const Signature& signature,
        std::string_view public_key
    ) -> bool;

    // ==================== Utility functions ====================

    /**
     * @brief Derive device ID from public key (SHA256 hash)
     *
     * @param public_key 32-byte public key
     * @return Result<std::string> Base64-encoded device ID
     */
    [[nodiscard]] static auto derive_device_id(std::string_view public_key) -> Result<std::string>;

    /**
     * @brief Validate public key format
     *
     * @param public_key Public key (Base64 or raw bytes)
     * @return bool Whether it's a valid public key
     */
    [[nodiscard]] static auto validate_public_key(std::string_view public_key) -> bool;

    /**
     * @brief Validate private key format
     *
     * @param private_key Private key (Base64 or raw bytes)
     * @return bool Whether it's a valid private key
     */
    [[nodiscard]] static auto validate_private_key(std::string_view private_key) -> bool;

    /**
     * @brief Convert public key to Base64
     *
     * @param public_key 32-byte public key
     * @return std::string Base64-encoded public key
     */
    [[nodiscard]] static auto public_key_to_base64(std::string_view public_key) -> std::string;

    /**
     * @brief Convert private key to Base64
     *
     * @param private_key 64-byte private key
     * @return std::string Base64-encoded private key
     */
    [[nodiscard]] static auto private_key_to_base64(std::string_view private_key) -> std::string;

    /**
     * @brief Convert Base64 to public key
     *
     * @param base64 Base64-encoded public key
     * @return Result<std::string> 32-byte raw public key, or error
     */
    [[nodiscard]] static auto public_key_from_base64(std::string_view base64) -> Result<std::string>;

    /**
     * @brief Convert Base64 to private key
     *
     * @param base64 Base64-encoded private key
     * @return Result<std::string> 64-byte raw private key, or error
     */
    [[nodiscard]] static auto private_key_from_base64(std::string_view base64) -> Result<std::string>;

    /**
     * @brief Constant-time comparison (prevent timing attacks)
     *
     * @param a Data A
     * @param b Data B
     * @param len Length
     * @return bool Whether equal
     */
    [[nodiscard]] static auto constant_time_compare(
        const std::byte* a,
        const std::byte* b,
        size_t len
    ) -> bool;

private:
    // Internal helper function
    [[nodiscard]] static auto compute_sha256(
        const std::byte* data,
        size_t len
    ) -> std::vector<std::byte>;
};

} // namespace moltcat::utils
