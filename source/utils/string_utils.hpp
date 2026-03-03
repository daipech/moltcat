#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace moltcat::utils {

/**
 * @brief String utility class
 *
 * Provides common string processing functions
 */
class StringUtils {
public:
    /**
     * @brief Check if string is empty or contains only whitespace
     */
    [[nodiscard]] static auto is_blank(std::string_view str) -> bool {
        return std::all_of(str.begin(), str.end(), [](char c) {
            return std::isspace(static_cast<unsigned char>(c));
        });
    }

    /**
     * @brief Remove whitespace from both ends of string
     */
    [[nodiscard]] static auto trim(std::string_view str) -> std::string {
        return trim_right(trim_left(str));
    }

    /**
     * @brief Remove whitespace from left side of string
     */
    [[nodiscard]] static auto trim_left(std::string_view str) -> std::string {
        auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char c) {
            return std::isspace(c);
        });
        return std::string(start, str.end());
    }

    /**
     * @brief Remove whitespace from right side of string
     */
    [[nodiscard]] static auto trim_right(std::string_view str) -> std::string {
        auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char c) {
            return std::isspace(c);
        }).base();
        return std::string(str.begin(), end);
    }

    /**
     * @brief Split string
     */
    [[nodiscard]] static auto split(std::string_view str,
                                   char delimiter,
                                   bool keep_empty = false) -> std::vector<std::string> {
        std::vector<std::string> result;
        split(str, delimiter, result, keep_empty);
        return result;
    }

    /**
     * @brief Split string (to output iterator)
     */
    template<typename OutputIt>
    static auto split(std::string_view str,
                     char delimiter,
                     OutputIt out,
                     bool keep_empty = false) -> void {
        std::string_view::size_type start = 0;
        auto end = str.find(delimiter);

        while (end != std::string_view::npos) {
            if (keep_empty || end > start) {
                *out++ = std::string(str.substr(start, end - start));
            }
            start = end + 1;
            end = str.find(delimiter, start);
        }

        if (keep_empty || start < str.size()) {
            *out++ = std::string(str.substr(start));
        }
    }

    /**
     * @brief Join strings
     */
    [[nodiscard]] static auto join(const std::vector<std::string>& strings,
                                  std::string_view delimiter) -> std::string {
        if (strings.empty()) {
            return "";
        }

        std::string result = strings[0];
        for (size_t i = 1; i < strings.size(); ++i) {
            result += delimiter;
            result += strings[i];
        }
        return result;
    }

    /**
     * @brief Convert to lowercase
     */
    [[nodiscard]] static auto to_lower(std::string_view str) -> std::string {
        std::string result(str.size(), '\0');
        std::transform(str.begin(), str.end(), result.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    /**
     * @brief Convert to uppercase
     */
    [[nodiscard]] static auto to_upper(std::string_view str) -> std::string {
        std::string result(str.size(), '\0');
        std::transform(str.begin(), str.end(), result.begin(),
                      [](unsigned char c) { return std::toupper(c); });
        return result;
    }

    /**
     * @brief Compare strings (case-insensitive)
     */
    [[nodiscard]] static auto equals_ignore_case(std::string_view lhs,
                                                 std::string_view rhs) -> bool {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        return std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                         [](char a, char b) {
                             return std::tolower(static_cast<unsigned char>(a)) ==
                                    std::tolower(static_cast<unsigned char>(b));
                         });
    }

    /**
     * @brief Check if string starts with prefix
     */
    [[nodiscard]] static auto starts_with(std::string_view str,
                                         std::string_view prefix) -> bool {
        return str.size() >= prefix.size() &&
               str.compare(0, prefix.size(), prefix) == 0;
    }

    /**
     * @brief Check if string ends with suffix
     */
    [[nodiscard]] static auto ends_with(std::string_view str,
                                       std::string_view suffix) -> bool {
        return str.size() >= suffix.size() &&
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    /**
     * @brief Check if string contains substring
     */
    [[nodiscard]] static auto contains(std::string_view str,
                                      std::string_view sub) -> bool {
        return str.find(sub) != std::string_view::npos;
    }

    /**
     * @brief Replace all occurrences of substring
     */
    [[nodiscard]] static auto replace(std::string_view str,
                                     std::string_view from,
                                     std::string_view to) -> std::string {
        if (from.empty()) {
            return std::string(str);
        }

        std::string result;
        std::string_view::size_type start = 0;
        auto end = str.find(from);

        while (end != std::string_view::npos) {
            result += str.substr(start, end - start);
            result += to;
            start = end + from.size();
            end = str.find(from, start);
        }

        result += str.substr(start);
        return result;
    }

    /**
     * @brief Format string (similar to Python's format)
     *
     * Use {} as placeholder
     * Example: format("Hello {}", "World") -> "Hello World"
     */
    template<typename... Args>
    [[nodiscard]] static auto format(std::string_view fmt, Args&&... args) -> std::string {
        return std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
    }

    /**
     * @brief Truncate string to specified length
     */
    [[nodiscard]] static auto truncate(std::string_view str,
                                     size_t max_length,
                                     std::string_view ellipsis = "...") -> std::string {
        if (str.size() <= max_length) {
            return std::string(str);
        }

        if (max_length <= ellipsis.size()) {
            return std::string(ellipsis.substr(0, max_length));
        }

        return std::string(str.substr(0, max_length - ellipsis.size())) + std::string(ellipsis);
    }

    /**
     * @brief Shorten string (remove middle part, keep both ends)
     */
    [[nodiscard]] static auto shorten(std::string_view str,
                                     size_t max_length,
                                     std::string_view ellipsis = "...") -> std::string {
        if (str.size() <= max_length) {
            return std::string(str);
        }

        if (max_length <= ellipsis.size()) {
            return std::string(ellipsis.substr(0, max_length));
        }

        size_t keep = (max_length - ellipsis.size()) / 2;
        return std::string(str.substr(0, keep)) + std::string(ellipsis) +
               std::string(str.substr(str.size() - keep));
    }

    /**
     * @brief Base64 encode
     */
    [[nodiscard]] static auto base64_encode(std::string_view str) -> std::string;

    /**
     * @brief Base64 decode
     */
    [[nodiscard]] static auto base64_decode(std::string_view str) -> std::string;

    /**
     * @brief URL encode
     */
    [[nodiscard]] static auto url_encode(std::string_view str) -> std::string;

    /**
     * @brief URL decode
     */
    [[nodiscard]] static auto url_decode(std::string_view str) -> std::string;
};

} // namespace moltcat::utils
