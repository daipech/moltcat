#include "auth_manager.hpp"
#include "signature_verifier.hpp"
#include "../utils/ed25519.hpp"
#include <shared_mutex>

namespace moltcat::gateway::auth {

AuthManager::AuthManager(std::shared_ptr<token::TokenManager> token_manager)
    : token_manager_(std::move(token_manager)) {

    if (!token_manager_) {
        throw std::invalid_argument("TokenManager cannot be null");
    }
}

auto AuthManager::verify_token(std::string_view token)
    -> Result<token::TokenInfo> {
    return token_manager_->verify_token(token);
}

auto AuthManager::verify_device_signature(
    std::string_view device_id,
    std::string_view public_key,
    std::string_view signature,
    std::string_view challenge_payload
) -> Result<bool> {
    // Use signature verifier
    SignatureVerifier verifier;

    // Verify signature
    auto verify_result = verifier.verify_device_signature(
        device_id,
        challenge_payload,
        signature
    );

    if (!verify_result.has_value()) {
        return Err<bool>(
            verify_result.error().code(),
            verify_result.error().message()
        );
    }

    // If signature is valid, register device (if not yet registered)
    if (verify_result.value()) {
        auto register_result = register_device(device_id, public_key);
        if (!register_result.has_value()) {
            // Registration failure does not affect verification result, but log error
            // In production, may need logging
        }
    }

    return Ok(verify_result.value());
}

auto AuthManager::issue_device_token(
    std::string_view device_id,
    std::string_view role,
    const std::vector<std::string>& scopes,
    uint64_t ttl_seconds
) -> Result<std::string> {
    // Verify device is registered
    if (!is_device_registered(device_id)) {
        return Err<std::string>(
            ErrorCode::INVALID_ARGUMENT,
            std::format("Device not registered: {}", device_id)
        );
    }

    // Verify role
    if (role != "operator" && role != "node") {
        return Err<std::string>(
            ErrorCode::INVALID_ARGUMENT,
            std::format("Invalid role: {}", role)
        );
    }

    // Issue Token
    return token_manager_->issue_device_token(
        device_id,
        role,
        scopes,
        {},     // caps
        {},     // commands
        {},     // permissions
        ttl_seconds
    );
}

auto AuthManager::rotate_device_token(
    std::string_view old_token,
    uint64_t ttl_seconds
) -> Result<std::string> {
    return token_manager_->rotate_device_token(old_token, ttl_seconds);
}

auto AuthManager::revoke_device_token(std::string_view token) -> Result<void> {
    return token_manager_->revoke_token(token);
}

auto AuthManager::register_device(std::string_view device_id, std::string_view public_key)
    -> Result<void> {
    // 验证设备 ID 格式
    if (!validate_device_id(device_id)) {
        return Err<void>(
            ErrorCode::INVALID_ARGUMENT,
            std::format("无效的设备 ID 格式：{}", device_id)
        );
    }

    // 验证公钥格式
    if (!utils::Ed25519::validate_public_key(public_key)) {
        return Err<void>(
            ErrorCode::INVALID_KEY_FORMAT,
            "无效的公钥格式"
        );
    }

    // 验证设备 ID 与公钥匹配
    auto derived_id = derive_device_id(public_key);
    if (!derived_id.has_value()) {
        return Err<void>(
            ErrorCode::CRYPTO_ERROR,
            derived_id.error().message()
        );
    }

    if (derived_id.value() != device_id) {
        return Err<void>(
            ErrorCode::INVALID_ARGUMENT,
            "设备 ID 与公钥不匹配"
        );
    }

    // 注册设备
    std::unique_lock lock(registry_mutex_);
    device_registry_[std::string(device_id)] = std::string(public_key);

    return Ok();
}

auto AuthManager::unregister_device(std::string_view device_id) -> Result<void> {
    std::unique_lock lock(registry_mutex_);
    device_registry_.erase(std::string(device_id));
    return Ok();
}

auto AuthManager::get_device_public_key(std::string_view device_id) const
    -> std::optional<std::string> {
    std::shared_lock lock(registry_mutex_);
    auto it = device_registry_.find(std::string(device_id));
    if (it != device_registry_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto AuthManager::is_device_registered(std::string_view device_id) const -> bool {
    std::shared_lock lock(registry_mutex_);
    return device_registry_.contains(std::string(device_id));
}

auto AuthManager::validate_device_id(std::string_view device_id) -> bool {
    // 设备 ID 应该是 Base64 编码的 SHA256 哈希（44 字符）
    // SHA256 = 32 字节，Base64 编码后约 44 字符（无填充）
    if (device_id.empty() || device_id.size() > 128) {
        return false;
    }

    // 检查是否为有效的 Base64 字符
    for (char c : device_id) {
        if (!std::isalnum(static_cast<unsigned char>(c)) &&
            c != '-' && c != '_') {
            return false;
        }
    }

    return true;
}

auto AuthManager::derive_device_id(std::string_view public_key) -> Result<std::string> {
    return utils::Ed25519::derive_device_id(public_key);
}

} // namespace moltcat::gateway::auth
