#pragma once

#include <string>
#include <string_view>
#include "../utils/ed25519.hpp"
#include "../utils/error.hpp"

namespace moltcat::gateway::auth {

/**
 * @brief 签名验证器
 *
 * 用于验证设备在 OpenClaw 协议中的身份签名
 */
class SignatureVerifier {
public:
    /**
     * @brief 验证设备签名
     *
     * 验证设备对挑战负载的签名是否有效
     *
     * @param device_id 设备 ID（Base64 编码的公钥哈希）
     * @param challenge_payload 挑战负载
     * @param base64_signature Base64 编码的签名
     * @return Result<bool> true 表示验证通过，false 表示失败，或错误
     */
    [[nodiscard]] auto verify_device_signature(
        std::string_view device_id,
        std::string_view challenge_payload,
        std::string_view base64_signature
    ) -> Result<bool>;

    /**
     * @brief 从公钥派生设备 ID
     *
     * @param base64_public_key Base64 编码的公钥
     * @return Result<std::string> Base64 编码的设备 ID
     */
    [[nodiscard]] static auto derive_device_id(std::string_view base64_public_key) -> Result<std::string>;

    /**
     * @brief 验证公钥格式
     *
     * @param base64_public_key Base64 编码的公钥
     * @return bool 是否为有效的公钥
     */
    [[nodiscard]] static auto validate_public_key(std::string_view base64_public_key) -> bool;

    /**
     * @brief 验证签名格式
     *
     * @param base64_signature Base64 编码的签名
     * @return bool 是否为有效的签名格式
     */
    [[nodiscard]] static auto validate_signature(std::string_view base64_signature) -> bool;

private:
    // 可选：缓存已验证的公钥（用于性能优化）
    // std::unordered_map<std::string, std::vector<std::byte>> public_key_cache_;
};

} // namespace moltcat::gateway::auth
