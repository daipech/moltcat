#include "json_utils.hpp"
#include "file_utils.hpp"
#include <fstream>

namespace moltcat::utils {

auto JsonUtils::load_from_file(std::string_view file_path) -> Result<glz::json_t> {
    // Read file content
    auto content_result = FileUtils::read_text_file(file_path);
    if (!content_result.has_value()) {
        return Err<glz::json_t>(content_result.error().code(),
                 std::format("Failed to read file: {}", file_path));
    }

    // Parse JSON
    return parse(content_result.value());
}

auto JsonUtils::save_to_file(std::string_view file_path,
                             const glz::json_t& json,
                             bool pretty) -> Result<void> {
    // Serialize JSON
    auto json_str_result = stringify(json, pretty);
    if (!json_str_result.has_value()) {
        return Err(json_str_result.error().code(),
                 std::format("JSON serialization failed: {}", file_path));
    }

    // Write to file
    return FileUtils::write_text_file(file_path, json_str_result.value());
}

auto JsonUtils::merge(glz::json_t& target,
                      const glz::json_t& source,
                      bool deep) -> void {
    if (!source.is_object() || !target.is_object()) {
        target = clone(source, deep);
        return;
    }

    auto& target_obj = target.get<glz::json_t::object_t>();
    const auto& source_obj = source.get<glz::json_t::object_t>();

    for (const auto& [key, value] : source_obj) {
        auto it = target_obj.find(key);

        if (it == target_obj.end()) {
            // Key doesn't exist, add directly
            target_obj[key] = clone(value, deep);
        } else if (deep && value.is_object() && it->second.is_object()) {
            // Recursively merge objects
            merge(it->second, value, deep);
        } else {
            // Overwrite value
            it->second = clone(value, deep);
        }
    }
}

} // namespace moltcat::utils
