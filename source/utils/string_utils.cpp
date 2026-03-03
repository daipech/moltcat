#include "string_utils.hpp"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <regex>

namespace moltcat::utils {

// Base64 encoding table
static constexpr char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// Base64 decoding table
static constexpr int base64_decode_table[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

auto StringUtils::base64_encode(std::string_view str) -> std::string {
    const auto* in = reinterpret_cast<const unsigned char*>(str.data());
    size_t in_len = str.size();

    std::string result;
    result.reserve(((in_len + 2) / 3) * 4);

    for (size_t i = 0; i < in_len; i += 3) {
        unsigned char byte0 = in[i];
        unsigned char byte1 = (i + 1 < in_len) ? in[i + 1] : 0;
        unsigned char byte2 = (i + 2 < in_len) ? in[i + 2] : 0;

        result.push_back(base64_chars[byte0 >> 2]);
        result.push_back(base64_chars[((byte0 & 0x03) << 4) | (byte1 >> 4)]);
        result.push_back((i + 1 < in_len) ? base64_chars[((byte1 & 0x0F) << 2) | (byte2 >> 6)] : '=');
        result.push_back((i + 2 < in_len) ? base64_chars[byte2 & 0x3F] : '=');
    }

    return result;
}

auto StringUtils::base64_decode(std::string_view str) -> std::string {
    // Remove padding characters
    std::string cleaned(str);
    while (!cleaned.empty() && cleaned.back() == '=') {
        cleaned.pop_back();
    }

    if (cleaned.size() % 4 != 0) {
        throw std::invalid_argument("Invalid Base64 string");
    }

    std::string result;
    result.reserve((cleaned.size() / 4) * 3);

    for (size_t i = 0; i < cleaned.size(); i += 4) {
        char c0 = cleaned[i];
        char c1 = cleaned[i + 1];
        char c2 = (i + 2 < cleaned.size()) ? cleaned[i + 2] : 0;
        char c3 = (i + 3 < cleaned.size()) ? cleaned[i + 3] : 0;

        int v0 = base64_decode_table[static_cast<unsigned char>(c0)];
        int v1 = base64_decode_table[static_cast<unsigned char>(c1)];
        int v2 = base64_decode_table[static_cast<unsigned char>(c2)];
        int v3 = base64_decode_table[static_cast<unsigned char>(c3)];

        if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0) {
            throw std::invalid_argument("Invalid Base64 character");
        }

        result.push_back(static_cast<char>((v0 << 2) | (v1 >> 4)));
        if (i + 2 < cleaned.size()) {
            result.push_back(static_cast<char>(((v1 & 0x0F) << 4) | (v2 >> 2)));
        }
        if (i + 3 < cleaned.size()) {
            result.push_back(static_cast<char>(((v2 & 0x03) << 6) | v3));
        }
    }

    return result;
}

auto StringUtils::url_encode(std::string_view str) -> std::string {
    std::ostringstream result;
    result.fill('0');
    result << std::hex;

    for (char c : str) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            result << c;
        } else {
            result << std::uppercase;
            result << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
            result << std::nouppercase;
        }
    }

    return result.str();
}

auto StringUtils::url_decode(std::string_view str) -> std::string {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            // Try to parse hexadecimal
            std::string hex = str.substr(i + 1, 2);
            char* end = nullptr;
            long val = std::strtol(hex.c_str(), &end, 16);

            if (end == hex.c_str() + 2) {
                result += static_cast<char>(val);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }

    return result;
}

} // namespace moltcat::utils
