#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <string>
#include <memory>
#include <source_location>

namespace moltcat::utils {

/**
 * @brief Log level
 */
enum class LogLevel {
    TRACE = SPDLOG_LEVEL_TRACE,
    DEBUG = SPDLOG_LEVEL_DEBUG,
    INFO = SPDLOG_LEVEL_INFO,
    WARN = SPDLOG_LEVEL_WARN,
    ERR = SPDLOG_LEVEL_ERROR,
    CRITICAL = SPDLOG_LEVEL_CRITICAL,
    OFF = SPDLOG_LEVEL_OFF
};

/**
 * @brief Logging system
 *
 * Wraps spdlog to provide a unified logging interface
 */
class Logger {
public:
    /**
     * @brief Get Logger instance (singleton)
     */
    static auto get_instance() -> Logger& {
        static Logger instance;
        return instance;
    }

    /**
     * @brief Initialize logging system
     *
     * @param name Logger name
     * @param level Log level
     * @param log_file Log file path (empty string means no file output)
     * @param max_file_size Maximum size of single log file (bytes)
     * @param max_files Number of log files to keep
     */
    auto initialize(std::string_view name = "moltcat",
                   LogLevel level = LogLevel::INFO,
                   std::string_view log_file = "",
                   size_t max_file_size = 1024 * 1024 * 5,  // 5MB
                   size_t max_files = 3) -> void {
        std::vector<spdlog::sink_ptr> sinks;

        // Console sink (with color)
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(static_cast<spdlog::level::level_enum>(level));
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v (%s:%#)");
        sinks.push_back(console_sink);

        // File sink (optional)
        if (!log_file.empty()) {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                std::string(log_file), max_file_size, max_files);
            file_sink->set_level(static_cast<spdlog::level::level_enum>(level));
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v (%s:%#)");
            sinks.push_back(file_sink);
        }

        // Create logger
        logger_ = std::make_shared<spdlog::logger>(std::string(name), sinks.begin(), sinks.end());
        logger_->set_level(static_cast<spdlog::level::level_enum>(level));
        logger_->flush_on(static_cast<spdlog::level::level_enum>(level));

        // Register to spdlog
        spdlog::register_logger(logger_);
        spdlog::set_default_logger(logger_);
    }

    /**
     * @brief Set log level
     */
    auto set_level(LogLevel level) -> void {
        if (logger_) {
            logger_->set_level(static_cast<spdlog::level::level_enum>(level));
        }
    }

    /**
     * @brief Flush log
     */
    auto flush() -> void {
        if (logger_) {
            logger_->flush();
        }
    }

    // Logging methods

    template<typename... Args>
    auto trace(std::string_view fmt, Args&&... args) -> void {
        logger_->trace(fmt.data(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    auto debug(std::string_view fmt, Args&&... args) -> void {
        logger_->debug(fmt.data(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    auto info(std::string_view fmt, Args&&... args) -> void {
        logger_->info(fmt.data(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    auto warn(std::string_view fmt, Args&&... args) -> void {
        logger_->warn(fmt.data(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    auto error(std::string_view fmt, Args&&... args) -> void {
        logger_->error(fmt.data(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    auto critical(std::string_view fmt, Args&&... args) -> void {
        logger_->critical(fmt.data(), std::forward<Args>(args)...);
    }

    /**
     * @brief Logging with source location
     */
    template<typename... Args>
    auto log(LogLevel level,
            std::source_location location = std::source_location::current(),
            std::string_view fmt = "",
            Args&&... args) -> void {
        auto msg = std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
        logger_->log(static_cast<spdlog::level::level_enum>(level),
                    "[{}:{}] {}", location.function_name(), location.line(), msg);
    }

private:
    Logger() = default;

    std::shared_ptr<spdlog::logger> logger_;
};

/**
 * @brief Convenience macro: get Logger instance
 */
#define MOLT_LOGGER moltcat::utils::Logger::get_instance()

/**
 * @brief Convenience macro: log with source location
 */
#define MOLT_LOG_TRACE(level, fmt, ...) \
    MOLT_LOGGER.log(level, std::source_location::current(), fmt, ##__VA_ARGS__)

} // namespace moltcat::utils
