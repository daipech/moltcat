#pragma once

#include <string>
#include <string_view>
#include <format>
#include <source_location>

namespace moltcat::utils {

/**
 * @brief Error code enumeration
 *
 * Defines all possible error types in the system
 */
enum class ErrorCode {
    // General errors (0-99)
    SUCCESS = 0,
    UNKNOWN_ERROR = 1,
    INVALID_ARGUMENT = 2,
    OUT_OF_MEMORY = 3,
    NOT_IMPLEMENTED = 4,
    TIMEOUT = 5,

    // Network/IO errors (100-199)
    NETWORK_ERROR = 100,
    CONNECTION_FAILED = 101,
    CONNECTION_CLOSED = 102,
    REQUEST_TIMEOUT = 103,
    INVALID_RESPONSE = 104,

    // Data storage errors (200-299)
    STORAGE_ERROR = 200,
    DATABASE_ERROR = 201,
    FILE_NOT_FOUND = 202,
    FILE_READ_ERROR = 203,
    FILE_WRITE_ERROR = 204,
    SERIALIZATION_ERROR = 205,

    // Agent errors (300-399)
    AGENT_NOT_FOUND = 300,
    AGENT_ALREADY_EXISTS = 301,
    AGENT_INITIALIZATION_FAILED = 302,
    SKILL_NOT_FOUND = 303,
    SKILL_EXECUTION_FAILED = 304,
    TASK_DISPATCH_FAILED = 305,

    // Memory system errors (400-499)
    MEMORY_NOT_FOUND = 400,
    MEMORY_STORE_FAILED = 401,
    MEMORY_RETRIEVE_FAILED = 402,
    EMBEDDING_FAILED = 403,
    VECTOR_SEARCH_FAILED = 404,

    // Routing/Scheduling errors (500-599)
    ROUTING_ERROR = 500,
    NO_AVAILABLE_AGENT = 501,
    QUEUE_FULL = 502,
    SCHEDULER_ERROR = 503,

    // Cryptography errors (600-699)
    CRYPTO_ERROR = 600,
    KEY_GENERATION_FAILED = 601,
    INVALID_KEY_FORMAT = 602,
    INVALID_SIGNATURE = 603,
    SIGNING_FAILED = 604,
    VERIFICATION_FAILED = 605,
};

/**
 * @brief Exception class
 *
 * Used to throw exceptions with error codes and detailed information
 */
class Exception : public std::exception {
public:
    Exception() = default;

    Exception(ErrorCode code,
             std::string message,
             std::source_location location = std::source_location::current())
        : code_(code)
        , message_(std::move(message))
        , location_(location)
        , full_message_(format_message()) {}

    ~Exception() override = default;

    // Get error code
    [[nodiscard]] auto code() const noexcept -> ErrorCode {
        return code_;
    }

    // Get error message (without location information)
    [[nodiscard]] auto what() const noexcept -> const char* override {
        return full_message_.c_str();
    }

    // Get short message
    [[nodiscard]] auto message() const noexcept -> std::string_view {
        return message_;
    }

    // Get source code location
    [[nodiscard]] auto location() const noexcept -> const std::source_location& {
        return location_;
    }

private:
    ErrorCode code_;
    std::string message_;
    std::source_location location_;
    std::string full_message_;

    [[nodiscard]] auto format_message() const -> std::string {
        std::string code_name = get_error_code_name(code_);
        if (location_.line() > 0) {
            return std::format("[{}:{}:{}] {} (ErrorCode: {})",
                location_.file_name(),
                location_.line(),
                location_.function_name(),
                message_,
                code_name);
        } else {
            return std::format("{} (ErrorCode: {})", message_, code_name);
        }
    }

    [[nodiscard]] static auto get_error_code_name(ErrorCode code) -> std::string {
        switch (code) {
            // General errors
            case ErrorCode::SUCCESS: return "SUCCESS";
            case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
            case ErrorCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
            case ErrorCode::OUT_OF_MEMORY: return "OUT_OF_MEMORY";
            case ErrorCode::NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
            case ErrorCode::TIMEOUT: return "TIMEOUT";

            // Network/IO errors
            case ErrorCode::NETWORK_ERROR: return "NETWORK_ERROR";
            case ErrorCode::CONNECTION_FAILED: return "CONNECTION_FAILED";
            case ErrorCode::CONNECTION_CLOSED: return "CONNECTION_CLOSED";
            case ErrorCode::REQUEST_TIMEOUT: return "REQUEST_TIMEOUT";
            case ErrorCode::INVALID_RESPONSE: return "INVALID_RESPONSE";

            // Data storage errors
            case ErrorCode::STORAGE_ERROR: return "STORAGE_ERROR";
            case ErrorCode::DATABASE_ERROR: return "DATABASE_ERROR";
            case ErrorCode::FILE_NOT_FOUND: return "FILE_NOT_FOUND";
            case ErrorCode::FILE_READ_ERROR: return "FILE_READ_ERROR";
            case ErrorCode::FILE_WRITE_ERROR: return "FILE_WRITE_ERROR";
            case ErrorCode::SERIALIZATION_ERROR: return "SERIALIZATION_ERROR";

            // Agent errors
            case ErrorCode::AGENT_NOT_FOUND: return "AGENT_NOT_FOUND";
            case ErrorCode::AGENT_ALREADY_EXISTS: return "AGENT_ALREADY_EXISTS";
            case ErrorCode::AGENT_INITIALIZATION_FAILED: return "AGENT_INITIALIZATION_FAILED";
            case ErrorCode::SKILL_NOT_FOUND: return "SKILL_NOT_FOUND";
            case ErrorCode::SKILL_EXECUTION_FAILED: return "SKILL_EXECUTION_FAILED";
            case ErrorCode::TASK_DISPATCH_FAILED: return "TASK_DISPATCH_FAILED";

            // Memory system errors
            case ErrorCode::MEMORY_NOT_FOUND: return "MEMORY_NOT_FOUND";
            case ErrorCode::MEMORY_STORE_FAILED: return "MEMORY_STORE_FAILED";
            case ErrorCode::MEMORY_RETRIEVE_FAILED: return "MEMORY_RETRIEVE_FAILED";
            case ErrorCode::EMBEDDING_FAILED: return "EMBEDDING_FAILED";
            case ErrorCode::VECTOR_SEARCH_FAILED: return "VECTOR_SEARCH_FAILED";

            // Routing/Scheduling errors
            case ErrorCode::ROUTING_ERROR: return "ROUTING_ERROR";
            case ErrorCode::NO_AVAILABLE_AGENT: return "NO_AVAILABLE_AGENT";
            case ErrorCode::QUEUE_FULL: return "QUEUE_FULL";
            case ErrorCode::SCHEDULER_ERROR: return "SCHEDULER_ERROR";

            // Cryptography errors
            case ErrorCode::CRYPTO_ERROR: return "CRYPTO_ERROR";
            case ErrorCode::KEY_GENERATION_FAILED: return "KEY_GENERATION_FAILED";
            case ErrorCode::INVALID_KEY_FORMAT: return "INVALID_KEY_FORMAT";
            case ErrorCode::INVALID_SIGNATURE: return "INVALID_SIGNATURE";
            case ErrorCode::SIGNING_FAILED: return "SIGNING_FAILED";
            case ErrorCode::VERIFICATION_FAILED: return "VERIFICATION_FAILED";

            default:
                return "UNKNOWN";
        }
    }
};

/**
 * @brief Result type - for return values or errors
 *
 * Similar to Rust's Result<T, E> or C++23's std::expected
 */
template<typename T>
class Result {
public:
    // Construct success result
    Result(T value) : value_(std::move(value)), has_value_(true) {}

    // Construct error result
    Result(ErrorCode code, std::string message)
        : error_(Exception(code, std::move(message)))
        , has_value_(false) {}

    Result(Exception exception)
        : error_(std::move(exception))
        , has_value_(false) {}

    ~Result() {
        if (has_value_) {
            value_.~T();
        } else {
            error_.~Exception();
        }
    }

    // Check if contains value
    [[nodiscard]] auto has_value() const noexcept -> bool {
        return has_value_;
    }

    // Explicit conversion to bool
    explicit operator bool() const noexcept {
        return has_value_;
    }

    // Get value (may throw exception)
    [[nodiscard]] auto value() const& -> const T& {
        if (!has_value_) {
            std::rethrow_exception(std::make_exception_ptr(error_));
        }
        return value_;
    }

    [[nodiscard]] auto value() && -> T {
        if (!has_value_) {
            std::rethrow_exception(std::make_exception_ptr(error_));
        }
        return std::move(value_);
    }

    // Get value or default value
    [[nodiscard]] auto value_or(T&& default_value) const& -> T {
        return has_value_ ? value_ : std::forward<T>(default_value);
    }

    // Get error
    [[nodiscard]] auto error() const& -> const Exception& {
        return error_;
    }

    // Arrow operator
    auto operator->() const -> const T* {
        return &value();
    }

    auto operator->() -> T* {
        return &value();
    }

    auto operator*() const& -> const T& {
        return value();
    }

    auto operator*() && -> T&& {
        return std::move(value());
    }

private:
    union {
        T value_;
        Exception error_;
    };
    bool has_value_;
};

// void specialization
template<>
class Result<void> {
public:
    Result() : has_value_(true), error_() {}

    Result(ErrorCode code, std::string message)
        : error_(Exception(code, std::move(message)))
        , has_value_(false) {}

    Result(Exception exception)
        : error_(std::move(exception))
        , has_value_(false) {}

    [[nodiscard]] auto has_value() const noexcept -> bool {
        return has_value_;
    }

    explicit operator bool() const noexcept {
        return has_value_;
    }

    void value() const {
        if (!has_value_) {
            std::rethrow_exception(std::make_exception_ptr(error_));
        }
    }

    [[nodiscard]] auto error() const& -> const Exception& {
        return error_;
    }

private:
    Exception error_;
    bool has_value_;
};

/**
 * @brief Helper function: create success result
 */
template<typename T>
auto Ok(T&& value) -> Result<std::decay_t<T>> {
    return Result<std::decay_t<T>>(std::forward<T>(value));
}

inline auto Ok() -> Result<void> {
    return Result<void>();
}

/**
 * @brief Helper function: create error result
 */
template<typename T = void>
auto Err(ErrorCode code, std::string message) -> Result<T> {
    if constexpr (std::is_void_v<T>) {
        return Result<T>(code, std::move(message));
    } else {
        return Result<T>(Exception(code, std::move(message)));
    }
}

} // namespace moltcat::utils
