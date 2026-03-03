#pragma once

#include "error.hpp"
#include <glaze/json.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <map>

namespace moltcat::utils {

/**
 * @brief JSON utility class
 *
 * Wraps glaze library to provide convenient JSON operation interfaces
 */
class JsonUtils {
public:
    /**
     * @brief Parse JSON string
     *
     * @param json_str JSON string
     * @return Result<glz::json_t> Parse result or error
     */
    [[nodiscard]] static auto parse(std::string_view json_str) -> Result<glz::json_t> {
        glz::json_t json;
        auto error = glz::read_json(json_str, json);

        if (error) {
            return Err<glz::json_t>(ErrorCode::SERIALIZATION_ERROR,
                     std::format("JSON parsing failed: {}", error));
        }

        return Ok(std::move(json));
    }

    /**
     * @brief Serialize JSON object to string
     *
     * @param json JSON object
     * @param pretty Whether to pretty print
     * @return Result<std::string> Serialization result or error
     */
    [[nodiscard]] static auto stringify(const glz::json_t& json,
                                       bool pretty = false) -> Result<std::string> {
        std::string result;
        if (pretty) {
            auto error = glz::write<glz::opts{.prettify = true, .indentation_width = 2}>(json, result);
            if (error) {
                return Err<std::string>(ErrorCode::SERIALIZATION_ERROR,
                         std::format("JSON serialization failed: {}", error));
            }
        } else {
            result = glz::write_json(json);
        }
        return Ok(result);
    }

    /**
     * @brief Load JSON from file
     *
     * @param file_path File path
     * @return Result<glz::json_t> Parse result or error
     */
    [[nodiscard]] static auto load_from_file(std::string_view file_path) -> Result<glz::json_t>;

    /**
     * @brief Save JSON to file
     *
     * @param file_path File path
     * @param json JSON object
     * @param pretty Whether to pretty print
     * @return Result<void> Success or error
     */
    [[nodiscard]] static auto save_to_file(std::string_view file_path,
                                          const glz::json_t& json,
                                          bool pretty = true) -> Result<void>;

    /**
     * @brief Get field value from JSON object
     *
     * @param json JSON object
     * @param key Key name (supports nesting, separated by ".", e.g., "user.name")
     * @param default_value Default value
     * @return Field value or default value
     */
    template<typename T>
    [[nodiscard]] static auto get_value(const glz::json_t& json,
                                       std::string_view key,
                                       const T& default_value) -> T {
        auto* ptr = get_pointer(json, key);
        if (ptr && ptr->holds<T>()) {
            return ptr->get<T>();
        }
        return default_value;
    }

    /**
     * @brief Get field value from JSON object (may not exist)
     */
    template<typename T>
    [[nodiscard]] static auto try_get_value(const glz::json_t& json,
                                           std::string_view key) -> std::optional<T> {
        auto* ptr = get_pointer(json, key);
        if (ptr && ptr->holds<T>()) {
            return ptr->get<T>();
        }
        return std::nullopt;
    }

    /**
     * @brief Set field value in JSON object
     *
     * @param json JSON object
     * @param key Key name (supports nesting, separated by ".")
     * @param value Value
     */
    template<typename T>
    static auto set_value(glz::json_t& json,
                         std::string_view key,
                         T&& value) -> void {
        auto keys = split_key(key);
        if (keys.empty()) {
            return;
        }

        glz::json_t* current = &json;

        // Traverse to the second-to-last level
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (!current->holds<glz::json::object_t>()) {
                *current = glz::json::object_t{};
            }

            auto& obj = current->get<glz::json::object_t>();
            auto it = obj.find(keys[i]);

            if (it == obj.end()) {
                // Create new object
                it = obj.emplace(keys[i], glz::json_t{}).first;
            }

            current = &it->second;
        }

        // Set the value at the last level
        if (!current->holds<glz::json::object_t>()) {
            *current = glz::json_t{};
        }

        auto& obj = current->get<glz::json::object_t>();
        obj[keys.back()] = std::forward<T>(value);
    }

    /**
     * @brief Merge two JSON objects
     *
     * @param target Target object
     * @param source Source object
     * @param deep Whether to deep copy
     */
    static auto merge(glz::json_t& target,
                     const glz::json_t& source,
                     bool deep = true) -> void;

    /**
     * @brief Clone JSON object
     * 
     * @param json JSON object to clone
     * @param deep Whether to perform deep copy (default: true)
     *             - true: recursively copy entire object tree
     *             - false: shallow copy (only top-level for objects/arrays)
     * @return Cloned JSON object
     */
    [[nodiscard]] static auto clone(const glz::json_t& json, bool deep = true) -> glz::json_t {
        if (deep) {
            // Deep copy: directly copy entire object
            return json;
        }
        
        // Shallow copy: only copy top level
        if (json.holds<glz::json_t::object_t>()) {
            // For objects: copy top-level keys only, nested objects are not recursively copied
            glz::json_t result = glz::json_t::object_t{};
            auto& result_obj = result.get<glz::json_t::object_t>();
            const auto& src_obj = json.get<glz::json_t::object_t>();
            
            for (const auto& [key, value] : src_obj) {
                result_obj[key] = value;
            }
            
            return result;
        } 
        else if (json.holds<glz::json_t::array_t>()) {
            // For arrays: copy array elements (element references)
            glz::json_t result = glz::json_t::array_t{};
            auto& result_arr = result.get<glz::json_t::array_t>();
            const auto& src_arr = json.get<glz::json_t::array_t>();
            
            result_arr.reserve(src_arr.size());
            for (const auto& element : src_arr) {
                result_arr.push_back(element);
            }
            
            return result;
        }
        
        // For primitive types: direct copy
        return json;
    }

    /**
     * @brief Check if JSON object contains specified key
     */
    [[nodiscard]] static auto has_key(const glz::json_t& json,
                                     std::string_view key) -> bool {
        return get_pointer(json, key) != nullptr;
    }

    /**
     * @brief Remove field from JSON object
     * 
     * @param json JSON object
     * @param key Key name (supports nesting, separated by ".")
     * @return true if key was removed, false if key doesn't exist
     */
    static auto remove(glz::json_t& json, std::string_view key) -> bool {
        if (key.empty()) {
            return false;
        }

        auto keys = split_key(key);
        if (keys.empty()) {
            return false;
        }

        // Single key - direct removal
        if (keys.size() == 1) {
            if (!json.holds<glz::json_t::object_t>()) {
                return false;
            }
            return json.get<glz::json_t::object_t>().erase(keys[0]) > 0;
        }

        // Navigate to parent object
        glz::json_t* parent = &json;
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (!parent->holds<glz::json_t::object_t>()) {
                return false;
            }
            
            auto& obj = parent->get<glz::json_t::object_t>();
            auto it = obj.find(keys[i]);
            if (it == obj.end()) {
                return false;
            }
            
            parent = &it->second;
        }

        // Remove from parent
        if (!parent->holds<glz::json_t::object_t>()) {
            return false;
        }
        
        return parent->get<glz::json_t::object_t>().erase(keys.back()) > 0;
    }

    /**
     * @brief Get all keys of JSON object
     * 
     * @param json JSON object
     * @param recursive Whether to recursively get nested keys (with "." path)
     * @param prefix Internal use - prefix for nested keys
     * @return Vector of key names (or paths if recursive)
     */
    [[nodiscard]] static auto keys(const glz::json_t& json, 
                                   bool recursive = false,
                                   std::string_view prefix = "") -> std::vector<std::string> {
        std::vector<std::string> result;

        if (!json.holds<glz::json_t::object_t>()) {
            return result;
        }

        const auto& obj = json.get<glz::json_t::object_t>();
        
        if (!recursive) {
            // Non-recursive: only top-level keys
            result.reserve(obj.size());
            for (const auto& [key, _] : obj) {
                result.push_back(key);
            }
        } else {
            // Recursive: include all nested keys with full paths
            for (const auto& [key, value] : obj) {
                std::string full_key = prefix.empty() 
                    ? key 
                    : std::string(prefix) + "." + key;
                
                result.push_back(full_key);
                
                // Recursively get nested keys
                if (value.holds<glz::json_t::object_t>()) {
                    auto nested = keys(value, true, full_key);
                    result.insert(result.end(), 
                                 std::make_move_iterator(nested.begin()),
                                 std::make_move_iterator(nested.end()));
                }
            }
        }

        return result;
    }

    /**
     * @brief Validate if JSON string is valid
     */
    [[nodiscard]] static auto is_valid(std::string_view json_str) -> bool {
        glz::json_t json;
        auto error = glz::read_json(json_str, json);
        return !error;
    }

private:
    /**
     * @brief Split key name (supports nested keys separated by ".")
     */
    [[nodiscard]] static auto split_key(std::string_view key) -> std::vector<std::string> {
        std::vector<std::string> result;
        std::string_view::size_type start = 0;
        auto end = key.find('.');

        while (end != std::string_view::npos) {
            result.emplace_back(key.substr(start, end - start));
            start = end + 1;
            end = key.find('.', start);
        }

        result.emplace_back(key.substr(start));
        return result;
    }

    /**
     * @brief Get pointer to nested field
     */
    [[nodiscard]] static auto get_pointer(const glz::json_t& json,
                                         std::string_view key) -> const glz::json_t* {
        auto keys = split_key(key);
        const glz::json_t* current = &json;

        for (const auto& k : keys) {
            if (!current->holds<glz::json_t::object_t>()) {
                return nullptr;
            }

            const auto& obj = current->get<glz::json_t::object_t>();
            auto it = obj.find(k);

            if (it == obj.end()) {
                return nullptr;
            }

            current = &it->second;
        }

        return current;
    }

    [[nodiscard]] static auto get_pointer(glz::json_t& json,
                                         std::string_view key) -> glz::json_t* {
        return const_cast<glz::json_t*>(get_pointer(std::as_const(json), key));
    }
};

} // namespace moltcat::utils
