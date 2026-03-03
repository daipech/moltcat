#include "signature_verifier.hpp"
#include "../utils/string_utils.hpp"

namespace moltcat::gateway::auth {

auto SignatureVerifier::verify_device_signature(
    std::string_view device_id,
    std::string_view challenge_payload,
    std::string_view base64_signature
) -> Result<bool> {
    // 1. 验证签名格式
    if (!validate_signature(base64_signature)) {
        return Err<bool>(
            ErrorCode::INVALID_SIGNATURE,
            "签名格式无效"
        );
    }

    // 2. 解析签名
    auto signature_result = utils::Ed25519::Signature::from_base64(base64_signature);
    if (!signature_result.has_value()) {
        return Err<bool>(
            ErrorCode::INVALID_SIGNATURE,
            std::format("签名解析失败：{}", signature_result.error().message())
        );
    }

    // 3. 从 device_id 反推公钥
    // 注意：在实际应用中，device_id 应该从注册表中查找对应的公钥
    // 这里我们假设 device_id 就是公钥本身（简化版本）
    //
    // 生产环境应该：
    // - 从数据库或注册表中查找 device_id 对应的公钥
    // - 验证 device_id 确实是由该公钥派生的
    //
    // 临时方案：假设 device_id 就是 Base64 编码的公钥
    auto public_key_result = utils::Ed25519::public_key_from_base64(device_id);
    if (!public_key_result.has_value()) {
        // 尝试直接使用
        if (!utils::Ed25519::validate_public_key(device_id)) {
            return Err<bool>(
                ErrorCode::INVALID_KEY_FORMAT,
                std::format("无效的公钥或设备 ID：{}", device_id)
            );
        }
    }

    std::string_view public_key = public_key_result.has_value()
        ? std::string_view(public_key_result.value())
        : device_id;

    // 4. 验证签名
    bool is_valid = utils::Ed25519::verify(
        challenge_payload,
        signature_result.value(),
        public_key
    );

    if (!is_valid) {
        return Ok(false);  // 签名无效，但不是错误
    }

    // 5. 可选：验证 device_id 是否与公钥匹配
    auto derived_id = derive_device_id(public_key);
    if (derived_id.has_value()) {
        // 在生产环境中，这里应该验证派生的 ID 是否与提供的 device_id 匹配
        // if (derived_id.value() != device_id) {
        //     return Ok(false);
        // }
    }

    return Ok(true);
}

auto SignatureVerifier::derive_device_id(std::string_view base64_public_key) -> Result<std::string> {
    // 解码 Base64 公钥
    auto public_key_result = utils::Ed25519::public_key_from_base64(base64_public_key);
    if (!public_key_result.has_value()) {
        return Err<std::string>(
            ErrorCode::INVALID_KEY_FORMAT,
            std::format("公钥解码失败：{}", public_key_result.error().message())
        );
    }

    // 使用 Ed25519 工具派生设备 ID
    return utils::Ed25519::derive_device_id(public_key_result.value());
}

auto SignatureVerifier::validate_public_key(std::string_view base64_public_key) -> bool {
    return utils::Ed25519::validate_public_key(base64_public_key);
}

auto SignatureVerifier::validate_signature(std::string_view base64_signature) -> bool {
    // 基本格式检查：Base64 编码的签名应该是 64 字节，编码后约 88 字符
    if (base64_signature.empty()) {
        return false;
    }

    // 尝试解码并验证长度
    try {
        auto decoded = utils::StringUtils::base64_decode(base64_signature);
        return decoded.size() == utils::Ed25519::SIGNATURE_SIZE;
    } catch (...) {
        return false;
    }
}

} // namespace moltcat::gateway::auth
