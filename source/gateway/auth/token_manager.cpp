#include "token_manager.hpp"
#include "../utils/ed25519.hpp"
#include "../utils/string_utils.hpp"
#include <glaze/json.hpp>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/sha.h>

namespace moltcat::gateway::token {

// ==================== TokenPayload Implementation ====================

auto TokenPayload::to_json() const -> std::string {
    glz::json_t json;
    json["device_id"] = device_id;
    json["role"] = role;
    json["scopes"] = scopes;
    json["caps"] = caps;
    json["commands"] = commands;
    json["permissions"] = permissions;
    json["iat"] = iat;
    json["exp"] = exp;

    std::string result;
    glz::write_json(json, result);
    return result;
}

auto TokenPayload::from_json(std::string_view json_str) -> Result<TokenPayload> {
    try {
        glz::json_t json;
        auto err = glz::read_json(json, json_str);
        if (err) {
            return Err<TokenPayload>(
                ErrorCode::INVALID_ARGUMENT,
                std::format("JSON parsing failed: {}", glz::format_error(err, json))
            );
        }

        TokenPayload payload;

        // Required fields
        if (auto it = json.find("device_id"); it != json.end()) {
            if (auto* s = it->get_if<std::string>()) {
                payload.device_id = *s;
            }
        } else {
            return Err<TokenPayload>(
                ErrorCode::INVALID_ARGUMENT,
                "Missing device_id field"
            );
        }

        if (auto it = json.find("role"); it != json.end()) {
            if (auto* s = it->get_if<std::string>()) {
                payload.role = *s;
            }
        } else {
            return Err<TokenPayload>(
                ErrorCode::INVALID_ARGUMENT,
                "Missing role field"
            );
        }

        if (auto it = json.find("iat"); it != json.end()) {
            if (auto* n = it->get_if<uint64_t>()) {
                payload.iat = *n;
            }
        }

        if (auto it = json.find("exp"); it != json.end()) {
            if (auto* n = it->get_if<uint64_t>()) {
                payload.exp = *n;
            }
        }

        // Optional fields
        if (auto it = json.find("scopes"); it != json.end()) {
            if (auto* arr = it->get_if<std::vector<std::string>>()) {
                payload.scopes = *arr;
            }
        }

        if (auto it = json.find("caps"); it != json.end()) {
            if (auto* arr = it->get_if<std::vector<std::string>>()) {
                payload.caps = *arr;
            }
        }

        if (auto it = json.find("commands"); it != json.end()) {
            if (auto* arr = it->get_if<std::vector<std::string>>()) {
                payload.commands = *arr;
            }
        }

        if (auto it = json.find("permissions"); it != json.end()) {
            if (auto* obj = it->get_if<std::map<std::string, bool>>()) {
                payload.permissions = *obj;
            }
        }

        return Ok(payload);
    } catch (const std::exception& e) {
        return Err<TokenPayload>(
            ErrorCode::INVALID_ARGUMENT,
            std::format("Parsing failed: {}", e.what())
        );
    }
}

auto TokenPayload::to_info() const -> TokenInfo {
    TokenInfo info;
    info.device_id = device_id;
    info.role = role;
    info.scopes = scopes;
    info.caps = caps;
    info.commands = commands;
    info.permissions = permissions;
    info.issued_at = iat;
    info.expires_at = exp;
    return info;
}

// ==================== TokenManager Implementation ====================

TokenManager::TokenManager(const Config& config)
    : config_(config) {

    // Generate Ed25519 key pair for signing JWT
    auto keypair_result = utils::Ed25519::generate_keypair();
    if (!keypair_result.has_value()) {
        throw std::runtime_error(
            std::format("Token key pair generation failed: {}",
                keypair_result.error().message())
        );
    }

    auto& keypair = keypair_result.value();

    // Move keys (steal ownership)
    private_key_ = std::move(keypair.private_key);
    public_key_ = std::move(keypair.public_key);

    // Save Base64-encoded public key
    std::string_view public_key_view(
        reinterpret_cast<const char*>(public_key_.data()),
        public_key_.size()
    );
    public_key_base64_ = utils::Ed25519::public_key_to_base64(public_key_view);
}

TokenManager::~TokenManager() {
    // Securely erase private key
    if (!private_key_.empty()) {
        sodium_memzero(private_key_.data(), private_key_.size());
    }
}

TokenManager::TokenManager(TokenManager&& other) noexcept
    : config_(std::move(other.config_))
    , private_key_(std::move(other.private_key_))
    , public_key_(std::move(other.public_key_))
    , public_key_base64_(std::move(other.public_key_base64_))
    , revoked_tokens_(std::move(other.revoked_tokens_))
{}

TokenManager& TokenManager::operator=(TokenManager&& other) noexcept {
    if (this != &other) {
        config_ = std::move(other.config_);
        private_key_ = std::move(other.private_key_);
        public_key_ = std::move(other.public_key_);
        public_key_base64_ = std::move(other.public_key_base64_);
        revoked_tokens_ = std::move(other.revoked_tokens_);
    }
    return *this;
}

auto TokenManager::issue_device_token(
    std::string_view device_id,
    std::string_view role,
    const std::vector<std::string>& scopes,
    const std::vector<std::string>& caps,
    const std::vector<std::string>& commands,
    const std::map<std::string, bool>& permissions,
    uint64_t ttl_seconds
) -> Result<std::string> {
    // Validate role
    if (role != "operator" && role != "node") {
        return Err<std::string>(
            ErrorCode::INVALID_ARGUMENT,
            std::format("Invalid role: {}, must be operator or node", role)
        );
    }

    // Set validity period
    if (ttl_seconds == 0) {
        ttl_seconds = config_.default_ttl_seconds;
    }
    if (ttl_seconds > config_.max_ttl_seconds) {
        ttl_seconds = config_.max_ttl_seconds;
    }

    // Current time (Unix timestamp, seconds)
    uint64_t now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now()
    );

    // Build Payload
    TokenPayload payload;
    payload.device_id = device_id;
    payload.role = role;
    payload.scopes = scopes;
    payload.caps = caps;
    payload.commands = commands;
    payload.permissions = permissions;
    payload.iat = now;
    payload.exp = now + ttl_seconds;

    // Generate JWT
    return generate_jwt(payload);
}

auto TokenManager::verify_token(std::string_view token) -> Result<TokenInfo> {
    // Check if revoked
    if (is_revoked(token)) {
        return Err<TokenInfo>(
            ErrorCode::VERIFICATION_FAILED,
            "Token has been revoked"
        );
    }

    // Parse JWT
    auto payload_result = parse_jwt(token);
    if (!payload_result.has_value()) {
        return Err<TokenInfo>(
            payload_result.error().code(),
            payload_result.error().message()
        );
    }

    auto& payload = payload_result.value();

    // Check expiration time
    uint64_t now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now()
    );

    if (payload.exp < now) {
        return Err<TokenInfo>(
            ErrorCode::VERIFICATION_FAILED,
            "Token has expired"
        );
    }

    return Ok(payload.to_info());
}

auto TokenManager::rotate_device_token(
    std::string_view old_token,
    uint64_t ttl_seconds
) -> Result<std::string> {
    // Verify old Token
    auto old_info = verify_token(old_token);
    if (!old_info.has_value()) {
        return Err<std::string>(
            old_info.error().code(),
            std::format("Old Token invalid: {}", old_info.error().message())
        );
    }

    // Revoke old Token
    auto revoke_result = revoke_token(old_token);
    if (!revoke_result.has_value()) {
        return Err<std::string>(revoke_result.error().code(), revoke_result.error().message());
    }

    // Issue new Token
    return issue_device_token(
        old_info.value().device_id,
        old_info.value().role,
        old_info.value().scopes,
        old_info.value().caps,
        old_info.value().commands,
        old_info.value().permissions,
        ttl_seconds
    );
}

auto TokenManager::revoke_token(std::string_view token) -> Result<void> {
    auto token_id = extract_token_id(token);

    std::unique_lock lock(revoked_mutex_);
    revoked_tokens_.insert(token_id);

    return Ok();
}

auto TokenManager::is_revoked(std::string_view token) const -> bool {
    auto token_id = extract_token_id(token);

    std::shared_lock lock(revoked_mutex_);
    return revoked_tokens_.contains(token_id);
}

auto TokenManager::get_public_key() const -> std::string {
    return public_key_base64_;
}

auto TokenManager::set_public_key(std::string_view public_key_base64) -> Result<void> {
    // Decode public key
    auto decode_result = utils::Ed25519::public_key_from_base64(public_key_base64);
    if (!decode_result.has_value()) {
        return Err<void>(
            ErrorCode::INVALID_KEY_FORMAT,
            decode_result.error().message()
        );
    }

    // Update public key
    auto& decoded = decode_result.value();
    public_key_.assign(decoded.begin(), decoded.end());
    public_key_base64_ = public_key_base64;

    // Note: do not update private key, only for verification
    return Ok();
}

// ==================== Internal Helper Methods ====================

auto TokenManager::generate_jwt(const TokenPayload& payload) const -> Result<std::string> {
    // 1. Build Header
    glz::json_t header;
    header["alg"] = "EdDSA";
    header["typ"] = "JWT";

    std::string header_str;
    glz::write_json(header, header_str);

    // 2. Build Payload
    std::string payload_str = payload.to_json();

    // 3. Base64 URL encode
    std::string header_b64 = base64_url_encode(header_str);
    std::string payload_b64 = base64_url_encode(payload_str);

    // 4. Build signing data
    std::string signing_data = header_b64 + "." + payload_b64;

    // 5. Sign
    std::string_view private_key_view(
        reinterpret_cast<const char*>(private_key_.data()),
        private_key_.size()
    );

    auto signature_result = utils::Ed25519::sign(signing_data, private_key_view);
    if (!signature_result.has_value()) {
        return Err<std::string>(
            ErrorCode::CRYPTO_ERROR,
            signature_result.error().message()
        );
    }

    // 6. Base64 URL encode signature
    std::string signature_bytes(
        reinterpret_cast<const char*>(signature_result.value().data.data()),
        signature_result.value().data.size()
    );
    std::string signature_b64 = base64_url_encode(signature_bytes);

    // 7. Assemble JWT
    return Ok(signing_data + "." + signature_b64);
}

auto TokenManager::parse_jwt(std::string_view token) const -> Result<TokenPayload> {
    // JWT format: header.payload.signature
    auto parts = utils::StringUtils::split(token, '.', false);

    if (parts.size() != 3) {
        return Err<TokenPayload>(
            ErrorCode::INVALID_ARGUMENT,
            "Invalid JWT format"
        );
    }

    const std::string& header_b64 = parts[0];
    const std::string& payload_b64 = parts[1];
    const std::string& signature_b64 = parts[2];

    // Verify signature
    if (!verify_jwt_signature(header_b64, payload_b64, signature_b64)) {
        return Err<TokenPayload>(
            ErrorCode::VERIFICATION_FAILED,
            "JWT signature verification failed"
        );
    }

    // Parse Payload
    auto payload_decode_result = base64_url_decode(payload_b64);
    if (!payload_decode_result.has_value()) {
        return Err<TokenPayload>(
            ErrorCode::INVALID_ARGUMENT,
            payload_decode_result.error().message()
        );
    }

    return TokenPayload::from_json(payload_decode_result.value());
}

auto TokenManager::verify_jwt_signature(
    std::string_view header_b64,
    std::string_view payload_b64,
    std::string_view signature_b64
) const -> bool {
    // Rebuild signing data
    std::string signing_data = std::string(header_b64) + "." + std::string(payload_b64);

    // Decode signature
    auto signature_decode_result = base64_url_decode(signature_b64);
    if (!signature_decode_result.has_value()) {
        return false;
    }

    // Parse signature
    auto signature_result = utils::Ed25519::Signature::from_base64(
        signature_decode_result.value()
    );
    if (!signature_result.has_value()) {
        return false;
    }

    // Verify signature
    std::string_view public_key_view(
        reinterpret_cast<const char*>(public_key_.data()),
        public_key_.size()
    );

    return utils::Ed25519::verify(
        signing_data,
        signature_result.value(),
        public_key_view
    );
}

auto TokenManager::extract_token_id(std::string_view token) -> std::string {
    // Calculate SHA256 hash of Token as ID
    std::vector<std::byte> hash(32);

    crypto_hash_sha256(
        reinterpret_cast<unsigned char*>(hash.data()),
        reinterpret_cast<const unsigned char*>(token.data()),
        token.size()
    );

    // Convert to hexadecimal string
    std::ostringstream oss;
    for (const auto& byte : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(static_cast<unsigned char>(byte));
    }

    return oss.str();
}

auto TokenManager::base64_url_encode(std::string_view data) -> std::string {
    // First perform standard Base64 encoding
    std::string encoded = utils::StringUtils::base64_encode(data);

    // Convert to Base64 URL-safe format
    std::replace(encoded.begin(), encoded.end(), '+', '-');
    std::replace(encoded.begin(), encoded.end(), '/', '_');

    // Remove trailing padding characters '='
    encoded.erase(std::remove(encoded.begin(), encoded.end(), '='), encoded.end());

    return encoded;
}

auto TokenManager::base64_url_decode(std::string_view data) -> Result<std::string> {
    std::string decoded(data);

    // Convert back to standard Base64 format
    std::replace(decoded.begin(), decoded.end(), '-', '+');
    std::replace(decoded.begin(), decoded.end(), '_', '/');

    // Add padding characters
    while (decoded.size() % 4 != 0) {
        decoded += '=';
    }

    try {
        return Ok(utils::StringUtils::base64_decode(decoded));
    } catch (const std::exception& e) {
        return Err<std::string>(
            ErrorCode::INVALID_ARGUMENT,
            std::format("Base64 URL decoding failed: {}", e.what())
        );
    }
}

} // namespace moltcat::gateway::token
