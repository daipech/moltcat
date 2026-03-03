#pragma once

#include <string>
#include <random>
#include <chrono>
#include <format>
#include <array>
#include <algorithm>
#include <cstdint>

namespace moltcat::utils {

/**
 * @brief UUID generator
 *
 * Generates version 4 (random) UUID
 */
class UUIDGenerator {
public:
    /**
     * @brief Generate new UUID string
     *
     * Format: 123e4567-e89b-12d3-a456-426614174000
     */
    [[nodiscard]] static auto generate() -> std::string {
        static UUIDGenerator instance;
        return instance.generate_uuid();
    }

    /**
     * @brief Generate UUID string without hyphens
     *
     * Format: 123e4567e89b12d3a456426614174000
     */
    [[nodiscard]] static auto generate_simple() -> std::string {
        auto uuid = generate();
        uuid.erase(std::remove(uuid.begin(), uuid.end(), '-'), uuid.end());
        return uuid;
    }

    /**
     * @brief Validate UUID format
     */
    [[nodiscard]] static auto is_valid(std::string_view uuid) -> bool {
        // Standard UUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        if (uuid.size() != 36) {
            return false;
        }

        for (size_t i = 0; i < uuid.size(); ++i) {
            if (i == 8 || i == 13 || i == 18 || i == 23) {
                if (uuid[i] != '-') {
                    return false;
                }
            } else {
                if (!is_hex_digit(uuid[i])) {
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * @brief Generate nil UUID (all zeros)
     */
    [[nodiscard]] static auto nil() -> std::string {
        return "00000000-0000-0000-0000-000000000000";
    }

private:
    UUIDGenerator() {
        // Use hardware random number generator (if available)
        std::random_device rd;
        rng_.seed(rd());
    }

    [[nodiscard]] auto generate_uuid() -> std::string {
        // Generate 16 bytes of random data
        std::array<uint8_t, 16> bytes{};
        std::uniform_int_distribution<uint32_t> dist(0, 255);

        for (auto& byte : bytes) {
            byte = static_cast<uint8_t>(dist(rng_));
        }

        // Set version and variant bits
        bytes[6] = (bytes[6] & 0x0F) | 0x40;  // Version 4
        bytes[8] = (bytes[8] & 0x3F) | 0x80;  // Variant

        // Format as UUID string
        return std::format("{:02x}{:02x}{:02x}{:02x}-"
                         "{:02x}{:02x}-"
                         "{:02x}{:02x}-"
                         "{:02x}{:02x}-"
                         "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                         bytes[0], bytes[1], bytes[2], bytes[3],
                         bytes[4], bytes[5],
                         bytes[6], bytes[7],
                         bytes[8], bytes[9],
                         bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    }

    [[nodiscard]] static auto is_hex_digit(char c) -> bool {
        return (c >= '0' && c <= '9') ||
               (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    }

    std::mt19937 rng_;
};

/**
 * @brief Convenience function: generate UUID
 */
inline auto generate_uuid() -> std::string {
    return UUIDGenerator::generate();
}

/**
 * @brief Convenience function: generate simple UUID
 */
inline auto generate_uuid_simple() -> std::string {
    return UUIDGenerator::generate_simple();
}

} // namespace moltcat::utils
