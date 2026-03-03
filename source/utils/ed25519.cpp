#include "ed25519.hpp"
#include "string_utils.hpp"
#include <sodium.h>
#include <cstring>
#include <algorithm>

namespace moltcat::utils {

// ==================== KeyPair destructor ====================

Ed25519::KeyPair::~KeyPair() {
    // Securely wipe private key
    if (!private_key.empty()) {
        sodium_memzero(private_key.data(), private_key.size());
    }
}

// ==================== Signature implementation ====================

auto Ed25519::Signature::to_base64() const -> std::string {
    // Convert byte array to string view
    std::string_view data_view(
        reinterpret_cast<const char*>(data.data()),
        data.size()
    );
    return StringUtils::base64_encode(data_view);
}

auto Ed25519::Signature::from_base64(std::string_view base64) -> Result<Signature> {
    try {
        auto decoded = StringUtils::base64_decode(base64);

        // Validate length
        if (decoded.size() != SIGNATURE_SIZE) {
            return Err<Signature>(
                ErrorCode::INVALID_SIGNATURE,
                std::format("Invalid signature length: expected {} bytes, got {} bytes",
                    SIGNATURE_SIZE, decoded.size())
            );
        }

        // Convert to byte array
        std::vector<std::byte> signature_bytes(SIGNATURE_SIZE);
        std::memcpy(signature_bytes.data(), decoded.data(), SIGNATURE_SIZE);

        return Ok(Signature(std::move(signature_bytes)));
    } catch (const std::exception& e) {
        return Err<Signature>(
            ErrorCode::INVALID_SIGNATURE,
            std::format("Base64 decode failed: {}", e.what())
        );
    }
}

// ==================== Key generation ====================

auto Ed25519::generate_keypair() -> Result<KeyPair> {
    // Ensure libsodium is initialized
    if (sodium_init() < 0) {
        return Err<KeyPair>(
            ErrorCode::KEY_GENERATION_FAILED,
            "libsodium initialization failed"
        );
    }

    // Allocate key space
    std::vector<std::byte> priv(PRIVATE_KEY_SIZE);
    std::vector<std::byte> pub(PUBLIC_KEY_SIZE);

    // Generate key pair
    if (crypto_sign_keypair(
            reinterpret_cast<unsigned char*>(pub.data()),
            reinterpret_cast<unsigned char*>(priv.data())) != 0) {
        sodium_memzero(priv.data(), priv.size());
        return Err<KeyPair>(
            ErrorCode::KEY_GENERATION_FAILED,
            "Key pair generation failed"
        );
    }

    return Ok(KeyPair(std::move(priv), std::move(pub)));
}

auto Ed25519::keypair_from_seed(std::string_view seed) -> Result<KeyPair> {
    // Validate seed length
    if (seed.size() != SEED_SIZE) {
        return Err<KeyPair>(
            ErrorCode::INVALID_KEY_FORMAT,
            std::format("Invalid seed length: expected {} bytes, got {} bytes",
                SEED_SIZE, seed.size())
        );
    }

    // Ensure libsodium is initialized
    if (sodium_init() < 0) {
        return Err<KeyPair>(
            ErrorCode::KEY_GENERATION_FAILED,
            "libsodium initialization failed"
        );
    }

    // Allocate key space
    std::vector<std::byte> priv(PRIVATE_KEY_SIZE);
    std::vector<std::byte> pub(PUBLIC_KEY_SIZE);

    // Generate key pair from seed
    if (crypto_sign_seed_keypair(
            reinterpret_cast<unsigned char*>(pub.data()),
            reinterpret_cast<unsigned char*>(priv.data()),
            reinterpret_cast<const unsigned char*>(seed.data())) != 0) {
        sodium_memzero(priv.data(), priv.size());
        return Err<KeyPair>(
            ErrorCode::KEY_GENERATION_FAILED,
            "Failed to generate key pair from seed"
        );
    }

    return Ok(KeyPair(std::move(priv), std::move(pub)));
}

auto Ed25519::extract_public_key(std::string_view private_key) -> Result<std::vector<std::byte>> {
    // Validate private key length
    if (private_key.size() != PRIVATE_KEY_SIZE) {
        return Err<std::vector<std::byte>>(
            ErrorCode::INVALID_KEY_FORMAT,
            std::format("Invalid private key length: expected {} bytes, got {} bytes",
                PRIVATE_KEY_SIZE, private_key.size())
        );
    }

    // Extract public key from private key (public key is the last 32 bytes of private key)
    std::vector<std::byte> public_key(PUBLIC_KEY_SIZE);
    std::memcpy(
        public_key.data(),
        reinterpret_cast<const std::byte*>(private_key.data()) + SEED_SIZE,
        PUBLIC_KEY_SIZE
    );

    return Ok(std::move(public_key));
}

// ==================== Signing ====================

auto Ed25519::sign(
    std::string_view message,
    std::string_view private_key
) -> Result<Signature> {
    return sign(
        reinterpret_cast<const std::byte*>(message.data()),
        message.size(),
        private_key
    );
}

auto Ed25519::sign(
    const std::byte* data,
    size_t len,
    std::string_view private_key
) -> Result<Signature> {
    // Validate private key length
    if (private_key.size() != PRIVATE_KEY_SIZE) {
        return Err<Signature>(
            ErrorCode::INVALID_KEY_FORMAT,
            std::format("Invalid private key length: expected {} bytes, got {} bytes",
                PRIVATE_KEY_SIZE, private_key.size())
        );
    }

    // Allocate signature space
    std::vector<std::byte> signature(SIGNATURE_SIZE);

    // Sign
    unsigned long long signed_len = 0;
    if (crypto_sign_detached(
            reinterpret_cast<unsigned char*>(signature.data()),
            &signed_len,
            reinterpret_cast<const unsigned char*>(data),
            len,
            reinterpret_cast<const unsigned char*>(private_key.data())) != 0) {
        sodium_memzero(signature.data(), signature.size());
        return Err<Signature>(
            ErrorCode::SIGNING_FAILED,
            "Signing operation failed"
        );
    }

    // Validate signature length
    if (signed_len != SIGNATURE_SIZE) {
        sodium_memzero(signature.data(), signature.size());
        return Err<Signature>(
            ErrorCode::SIGNING_FAILED,
            std::format("Abnormal signature length: expected {} bytes, got {} bytes",
                SIGNATURE_SIZE, signed_len)
        );
    }

    return Ok(Signature(std::move(signature)));
}

// ==================== Verification ====================

auto Ed25519::verify(
    std::string_view message,
    const Signature& signature,
    std::string_view public_key
) -> bool {
    return verify(
        reinterpret_cast<const std::byte*>(message.data()),
        message.size(),
        signature,
        public_key
    );
}

auto Ed25519::verify(
    const std::byte* data,
    size_t len,
    const Signature& signature,
    std::string_view public_key
) -> bool {
    // Validate public key length
    if (public_key.size() != PUBLIC_KEY_SIZE) {
        return false;
    }

    // Validate signature length
    if (!signature.is_valid()) {
        return false;
    }

    // Verify signature
    int result = crypto_sign_verify_detached(
        reinterpret_cast<const unsigned char*>(signature.data.data()),
        reinterpret_cast<const unsigned char*>(data),
        len,
        reinterpret_cast<const unsigned char*>(public_key.data())
    );

    return result == 0;
}

// ==================== Utility functions ====================

auto Ed25519::derive_device_id(std::string_view public_key) -> Result<std::string> {
    // Validate public key length
    if (public_key.size() != PUBLIC_KEY_SIZE) {
        return Err<std::string>(
            ErrorCode::INVALID_KEY_FORMAT,
            std::format("Invalid public key length: expected {} bytes, got {} bytes",
                PUBLIC_KEY_SIZE, public_key.size())
        );
    }

    // Calculate SHA256 hash
    auto hash = compute_sha256(
        reinterpret_cast<const std::byte*>(public_key.data()),
        public_key.size()
    );

    // Convert to Base64
    std::string_view hash_view(
        reinterpret_cast<const char*>(hash.data()),
        hash.size()
    );

    return Ok(StringUtils::base64_encode(hash_view));
}

auto Ed25519::validate_public_key(std::string_view public_key) -> bool {
    // If Base64 encoded, decode first
    if (public_key.size() > PUBLIC_KEY_SIZE) {
        try {
            auto decoded = StringUtils::base64_decode(public_key);
            return decoded.size() == PUBLIC_KEY_SIZE;
        } catch (...) {
            return false;
        }
    }

    return public_key.size() == PUBLIC_KEY_SIZE;
}

auto Ed25519::validate_private_key(std::string_view private_key) -> bool {
    // If Base64 encoded, decode first
    if (private_key.size() > PRIVATE_KEY_SIZE) {
        try {
            auto decoded = StringUtils::base64_decode(private_key);
            return decoded.size() == PRIVATE_KEY_SIZE;
        } catch (...) {
            return false;
        }
    }

    return private_key.size() == PRIVATE_KEY_SIZE;
}

auto Ed25519::public_key_to_base64(std::string_view public_key) -> std::string {
    return StringUtils::base64_encode(public_key);
}

auto Ed25519::private_key_to_base64(std::string_view private_key) -> std::string {
    return StringUtils::base64_encode(private_key);
}

auto Ed25519::public_key_from_base64(std::string_view base64) -> Result<std::string> {
    try {
        auto decoded = StringUtils::base64_decode(base64);

        if (decoded.size() != PUBLIC_KEY_SIZE) {
            return Err<std::string>(
                ErrorCode::INVALID_KEY_FORMAT,
                std::format("Invalid public key length: expected {} bytes, got {} bytes",
                    PUBLIC_KEY_SIZE, decoded.size())
            );
        }

        return Ok(decoded);
    } catch (const std::exception& e) {
        return Err<std::string>(
            ErrorCode::INVALID_KEY_FORMAT,
            std::format("Base64 decode failed: {}", e.what())
        );
    }
}

auto Ed25519::private_key_from_base64(std::string_view base64) -> Result<std::string> {
    try {
        auto decoded = StringUtils::base64_decode(base64);

        if (decoded.size() != PRIVATE_KEY_SIZE) {
            return Err<std::string>(
                ErrorCode::INVALID_KEY_FORMAT,
                std::format("Invalid private key length: expected {} bytes, got {} bytes",
                    PRIVATE_KEY_SIZE, decoded.size())
            );
        }

        return Ok(decoded);
    } catch (const std::exception& e) {
        return Err<std::string>(
            ErrorCode::INVALID_KEY_FORMAT,
            std::format("Base64 decode failed: {}", e.what())
        );
    }
}

auto Ed25519::constant_time_compare(
    const std::byte* a,
    const std::byte* b,
    size_t len
) -> bool {
    // Use libsodium's constant-time comparison function
    return sodium_memcmp(
        reinterpret_cast<const unsigned char*>(a),
        reinterpret_cast<const unsigned char*>(b),
        len
    ) == 0;
}

auto Ed25519::compute_sha256(
    const std::byte* data,
    size_t len
) -> std::vector<std::byte> {
    // SHA256 output length is 32 bytes
    std::vector<std::byte> hash(32);

    crypto_hash_sha256(
        reinterpret_cast<unsigned char*>(hash.data()),
        reinterpret_cast<const unsigned char*>(data),
        len
    );

    return hash;
}

} // namespace moltcat::utils
