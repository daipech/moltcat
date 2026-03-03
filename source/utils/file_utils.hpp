#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include "error.hpp"

namespace moltcat::utils {

/**
 * @brief File system utility class
 *
 * Provides cross-platform file operation functionality
 */
class FileUtils {
public:
    /**
     * @brief Check if file exists
     */
    [[nodiscard]] static auto exists(std::string_view path) -> bool {
        return std::filesystem::exists(std::filesystem::path(path));
    }

    /**
     * @brief Check if it is a regular file
     */
    [[nodiscard]] static auto is_file(std::string_view path) -> bool {
        return std::filesystem::is_regular_file(std::filesystem::path(path));
    }

    /**
     * @brief Check if it is a directory
     */
    [[nodiscard]] static auto is_directory(std::string_view path) -> bool {
        return std::filesystem::is_directory(std::filesystem::path(path));
    }

    /**
     * @brief Get file size (bytes)
     */
    [[nodiscard]] static auto file_size(std::string_view path) -> Result<uintmax_t> {
        try {
            return Ok(std::filesystem::file_size(std::filesystem::path(path)));
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<uintmax_t>(ErrorCode::FILE_NOT_FOUND, e.what());
        }
    }

    /**
     * @brief Create directory (including parent directories)
     */
    static auto create_directory(std::string_view path) -> Result<void> {
        try {
            std::filesystem::create_directories(std::filesystem::path(path));
            return Ok();
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<void>(ErrorCode::STORAGE_ERROR, e.what());
        }
    }

    /**
     * @brief Delete file
     */
    static auto remove_file(std::string_view path) -> Result<void> {
        try {
            std::filesystem::remove(std::filesystem::path(path));
            return Ok();
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<void>(ErrorCode::STORAGE_ERROR, e.what());
        }
    }

    /**
     * @brief Delete directory and its contents
     */
    static auto remove_directory(std::string_view path, bool recursive = true) -> Result<void> {
        try {
            if (recursive) {
                std::filesystem::remove_all(std::filesystem::path(path));
            } else {
                std::filesystem::remove(std::filesystem::path(path));
            }
            return Ok();
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<void>(ErrorCode::STORAGE_ERROR, e.what());
        }
    }

    /**
     * @brief Copy file
     */
    static auto copy_file(std::string_view src, std::string_view dst) -> Result<void> {
        try {
            std::filesystem::copy_file(std::filesystem::path(src),
                                       std::filesystem::path(dst));
            return Ok();
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<void>(ErrorCode::STORAGE_ERROR, e.what());
        }
    }

    /**
     * @brief Move/rename file
     */
    static auto rename(std::string_view src, std::string_view dst) -> Result<void> {
        try {
            std::filesystem::rename(std::filesystem::path(src),
                                   std::filesystem::path(dst));
            return Ok();
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<void>(ErrorCode::STORAGE_ERROR, e.what());
        }
    }

    /**
     * @brief Read text file
     */
    [[nodiscard]] static auto read_text_file(std::string_view path) -> Result<std::string> {
        try {
            std::ifstream file{std::filesystem::path(path)};
            if (!file.is_open()) {
                return Err<std::string>(ErrorCode::FILE_READ_ERROR,
                         std::format("Failed to open file: {}", path));
            }

            std::string content;
            file.seekg(0, std::ios::end);
            content.reserve(file.tellg());
            file.seekg(0, std::ios::beg);

            content.assign((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

            return Ok(content);
        } catch (const std::exception& e) {
            return Err<std::string>(ErrorCode::FILE_READ_ERROR,
                     std::format("Failed to read file: {} - {}", path, e.what()));
        }
    }

    /**
     * @brief Write text file
     */
    static auto write_text_file(std::string_view path,
                                std::string_view content,
                                bool append = false) -> Result<void> {
        try {
            auto mode = append ? std::ios::app : std::ios::trunc;

            std::ofstream file{std::filesystem::path(path), mode};
            if (!file.is_open()) {
                return Err(ErrorCode::FILE_WRITE_ERROR,
                         std::format("Failed to create file: {}", path));
            }

            file << content;
            return Ok();
        } catch (const std::exception& e) {
            return Err(ErrorCode::FILE_WRITE_ERROR,
                     std::format("Failed to write file: {} - {}", path, e.what()));
        }
    }

    /**
     * @brief Read binary file
     */
    [[nodiscard]] static auto read_binary_file(std::string_view path) -> Result<std::vector<uint8_t>> {
        try {
            std::ifstream file{std::filesystem::path(path), std::ios::binary};
            if (!file.is_open()) {
                return Err<std::vector<uint8_t>>(ErrorCode::FILE_READ_ERROR,
                         std::format("Failed to open file: {}", path));
            }

            file.seekg(0, std::ios::end);
            auto size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<uint8_t> buffer(size);
            file.read(reinterpret_cast<char*>(buffer.data()), size);

            return Ok(buffer);
        } catch (const std::exception& e) {
            return Err<std::vector<uint8_t>>(ErrorCode::FILE_READ_ERROR,
                     std::format("Failed to read binary file: {} - {}", path, e.what()));
        }
    }

    /**
     * @brief Write binary file
     */
    static auto write_binary_file(std::string_view path,
                                  const std::vector<uint8_t>& data,
                                  bool append = false) -> Result<void> {
        try {
            auto mode = std::ios::binary | (append ? std::ios::app : std::ios::trunc);

            std::ofstream file{std::filesystem::path(path), mode};
            if (!file.is_open()) {
                return Err<void>(ErrorCode::FILE_WRITE_ERROR,
                         std::format("Failed to create file: {}", path));
            }

            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            return Ok();
        } catch (const std::exception& e) {
            return Err<void>(ErrorCode::FILE_WRITE_ERROR,
                     std::format("Failed to write binary file: {} - {}", path, e.what()));
        }
    }

    /**
     * @brief List directory contents
     */
    [[nodiscard]] static auto list_directory(std::string_view path) -> Result<std::vector<std::string>> {
        try {
            std::vector<std::string> result;

            for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(path))) {
                result.push_back(entry.path().string());
            }

            return Ok(result);
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<std::vector<std::string>>(ErrorCode::STORAGE_ERROR, e.what());
        }
    }

    /**
     * @brief List directory contents (filenames only)
     */
    [[nodiscard]] static auto list_directory_names(std::string_view path) -> Result<std::vector<std::string>> {
        try {
            std::vector<std::string> result;

            for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(path))) {
                result.push_back(entry.path().filename().string());
            }

            return Ok(result);
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<std::vector<std::string>>(ErrorCode::STORAGE_ERROR, e.what());
        }
    }

    /**
     * @brief Get file extension
     */
    [[nodiscard]] static auto get_extension(std::string_view path) -> std::string {
        return std::filesystem::path(path).extension().string();
    }

    /**
     * @brief Get filename (without path)
     */
    [[nodiscard]] static auto get_filename(std::string_view path) -> std::string {
        return std::filesystem::path(path).filename().string();
    }

    /**
     * @brief Get filename (without extension)
     */
    [[nodiscard]] static auto get_stem(std::string_view path) -> std::string {
        return std::filesystem::path(path).stem().string();
    }

    /**
     * @brief Get parent directory path
     */
    [[nodiscard]] static auto get_parent_path(std::string_view path) -> std::string {
        return std::filesystem::path(path).parent_path().string();
    }

    /**
     * @brief Join paths
     */
    [[nodiscard]] static auto join(std::string_view path1, std::string_view path2) -> std::string {
        return (std::filesystem::path(path1) / std::filesystem::path(path2)).string();
    }

    /**
     * @brief Get current working directory
     */
    [[nodiscard]] static auto current_path() -> Result<std::string> {
        try {
            return Ok(std::filesystem::current_path().string());
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<std::string>(ErrorCode::STORAGE_ERROR, e.what());
        }
    }

    /**
     * @brief Set current working directory
     */
    static auto set_current_path(std::string_view path) -> Result<void> {
        try {
            std::filesystem::current_path(std::filesystem::path(path));
            return Ok();
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<void>(ErrorCode::STORAGE_ERROR, e.what());
        }
    }

    /**
     * @brief Get absolute path
     */
    [[nodiscard]] static auto absolute_path(std::string_view path) -> Result<std::string> {
        try {
            return Ok(std::filesystem::absolute(std::filesystem::path(path)).string());
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<std::string>(ErrorCode::STORAGE_ERROR, e.what());
        }
    }

    /**
     * @brief Get relative path
     */
    [[nodiscard]] static auto relative_path(std::string_view path, std::string_view base) -> Result<std::string> {
        try {
            return Ok(std::filesystem::relative(std::filesystem::path(path),
                                                std::filesystem::path(base)).string());
        } catch (const std::filesystem::filesystem_error& e) {
            return Err<std::string>(ErrorCode::STORAGE_ERROR, e.what());
        }
    }

    /**
     * @brief Normalize path (remove . and .. etc.)
     */
    [[nodiscard]] static auto normalize_path(std::string_view path) -> std::string {
        return std::filesystem::path(path).lexically_normal().string();
    }
};

} // namespace moltcat::utils
