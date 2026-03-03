#pragma once

#include "../utils/uuid.hpp"
#include "../model/result.hpp"
#include <glaze/json.hpp>
#include <string>
#include <optional>
#include <shared_mutex>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <chrono>
#include <thread>
#include <unordered_map>

namespace moltcat::messaging {

// ================================
// Context Template Interface
// ================================

/**
 * @brief Context template interface
 *
 * @tparam ResultType Result type (e.g., model::Result, A2ATask, std::string, etc.)
 *
 * Design principles:
 * 1. Based on Golang context package design philosophy
 * 2. Use project's unified naming convention (get_*|is_* prefix)
 * 3. Support templated Result type
 */
template<typename ResultType>
class Context {
public:
    virtual ~Context() = default;

    // ========== Core methods (referencing Golang, naming style adjusted) ==========

    /**
     * @brief Get context deadline
     */
    [[nodiscard]] virtual auto get_deadline() const noexcept
        -> std::optional<uint64_t> = 0;

    /**
     * @brief Check if context is canceled/timed out
     */
    [[nodiscard]] virtual auto is_done() const noexcept -> bool = 0;

    /**
     * @brief Get reason for context cancellation
     */
    [[nodiscard]] virtual auto get_error() const noexcept
        -> std::optional<std::string> = 0;

    /**
     * @brief Get context associated value
     */
    [[nodiscard]] virtual auto get_value(std::string_view key) const noexcept
        -> std::optional<glz::json_t> = 0;

    // ========== Extension methods (MoltCat specific) ==========

    /**
     * @brief Get context ID
     */
    [[nodiscard]] virtual auto get_id() const noexcept
        -> const std::string& = 0;

    /**
     * @brief Get parent context
     */
    [[nodiscard]] virtual auto get_parent() const noexcept
        -> std::shared_ptr<Context<ResultType>> = 0;

    // ========== Result management (MoltCat extension, templated) ==========

    /**
     * @brief Set execution result
     */
    virtual auto set_result(const ResultType& result) -> void = 0;

    /**
     * @brief Get execution result
     */
    [[nodiscard]] virtual auto get_result(int timeout_ms = -1) const noexcept
        -> std::optional<ResultType> = 0;
};

// ================================
// EmptyContext (empty context)
// ================================

/**
 * @brief Empty context (never cancels)
 *
 * Use cases:
 * - As root context
 * - Never cancels, no deadline
 * - Carries no information
 */
template<typename ResultType>
class EmptyContext : public Context<ResultType> {
public:
    EmptyContext()
        : id_(utils::UUID::generate_v4())
    {}

    // ========== Context interface implementation ==========

    [[nodiscard]] auto get_deadline() const noexcept
        -> std::optional<uint64_t> override {
        return std::nullopt;  // No deadline
    }

    [[nodiscard]] auto is_done() const noexcept -> bool override {
        return false;  // Never cancels
    }

    [[nodiscard]] auto get_error() const noexcept
        -> std::optional<std::string> override {
        return std::nullopt;  // No error
    }

    [[nodiscard]] auto get_value(std::string_view key) const noexcept
        -> std::optional<glz::json_t> override {
        (void)key;
        return std::nullopt;  // No value
    }

    [[nodiscard]] auto get_id() const noexcept
        -> const std::string& override {
        return id_;
    }

    [[nodiscard]] auto get_parent() const noexcept
        -> std::shared_ptr<Context<ResultType>> override {
        return nullptr;  // No parent context
    }

    // ========== Result management ==========

    auto set_result(const ResultType& result) -> void override {
        std::unique_lock lock(mutex_);
        result_ = result;
        cv_.notify_all();
    }

    [[nodiscard]] auto get_result(int timeout_ms = -1) const noexcept
        -> std::optional<ResultType> override {
        std::unique_lock lock(mutex_);

        if (timeout_ms < 0) {
            cv_.wait(lock, [this] { return result_.has_value(); });
        } else {
            cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                        [this] { return result_.has_value(); });
        }

        return result_;
    }

private:
    std::string id_;
    std::optional<ResultType> result_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
};

// ================================
// CancelContext (cancelable context)
// ================================

/**
 * @brief Cancelable context
 *
 * Use cases:
 * - Can be manually canceled
 * - Used for interruptible async operations
 */
template<typename ResultType>
class CancelContext : public Context<ResultType> {
public:
    explicit CancelContext(std::shared_ptr<Context<ResultType>> parent)
        : parent_(std::move(parent))
        , id_(utils::UUID::generate_v4())
        , done_(false)
    {}

    /**
     * @brief Manually cancel context
     */
    auto cancel(const std::string& reason = "canceled") -> void {
        std::unique_lock lock(mutex_);
        if (!done_) {
            done_ = true;
            error_ = reason;
            cv_.notify_all();
        }
    }

    // ========== Context interface implementation ==========

    [[nodiscard]] auto get_deadline() const noexcept
        -> std::optional<uint64_t> override {
        return parent_ ? parent_->get_deadline() : std::nullopt;
    }

    [[nodiscard]] auto is_done() const noexcept -> bool override {
        return done_ || (parent_ && parent_->is_done());
    }

    [[nodiscard]] auto get_error() const noexcept
        -> std::optional<std::string> override {
        if (done_) {
            return error_;
        }
        return parent_ ? parent_->get_error() : std::nullopt;
    }

    [[nodiscard]] auto get_value(std::string_view key) const noexcept
        -> std::optional<glz::json_t> override {
        return parent_ ? parent_->get_value(key) : std::nullopt;
    }

    [[nodiscard]] auto get_id() const noexcept
        -> const std::string& override {
        return id_;
    }

    [[nodiscard]] auto get_parent() const noexcept
        -> std::shared_ptr<Context<ResultType>> override {
        return parent_;
    }

    // ========== Result management ==========

    auto set_result(const ResultType& result) -> void override {
        std::unique_lock lock(mutex_);
        result_ = result;
        cv_.notify_all();
    }

    [[nodiscard]] auto get_result(int timeout_ms = -1) const noexcept
        -> std::optional<ResultType> override {
        std::unique_lock lock(mutex_);

        if (timeout_ms < 0) {
            cv_.wait(lock, [this] { return result_.has_value() || done_; });
        } else {
            cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                        [this] { return result_.has_value() || done_; });
        }

        return result_;
    }

private:
    std::shared_ptr<Context<ResultType>> parent_;
    std::string id_;
    std::atomic<bool> done_;
    std::string error_;
    std::optional<ResultType> result_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
};

// ================================
// TimeoutContext (timeout context)
// ================================

/**
 * @brief Context with timeout
 *
 * Use cases:
 * - Auto-cancel after timeout
 * - Used for time-limited operations
 */
template<typename ResultType>
class TimeoutContext : public Context<ResultType> {
public:
    TimeoutContext(
        std::shared_ptr<Context<ResultType>> parent,
        uint64_t timeout_ms
    )
        : parent_(std::move(parent))
        , id_(utils::UUID::generate_v4())
        , deadline_ms_(get_current_time_ms() + timeout_ms)
        , done_(false)
    {
        start_timer();
    }

    ~TimeoutContext() override {
        // Stop timer thread
        {
            std::unique_lock lock(mutex_);
            done_ = true;
            cv_.notify_all();
        }
        if (timer_thread_.joinable()) {
            timer_thread_.join();
        }
    }

    // ========== Context interface implementation ==========

    [[nodiscard]] auto get_deadline() const noexcept
        -> std::optional<uint64_t> override {
        return deadline_ms_;
    }

    [[nodiscard]] auto is_done() const noexcept -> bool override {
        return done_ || (parent_ && parent_->is_done());
    }

    [[nodiscard]] auto get_error() const noexcept
        -> std::optional<std::string> override {
        if (done_ && error_) {
            return error_;
        }
        return parent_ ? parent_->get_error() : std::nullopt;
    }

    [[nodiscard]] auto get_value(std::string_view key) const noexcept
        -> std::optional<glz::json_t> override {
        return parent_ ? parent_->get_value(key) : std::nullopt;
    }

    [[nodiscard]] auto get_id() const noexcept
        -> const std::string& override {
        return id_;
    }

    [[nodiscard]] auto get_parent() const noexcept
        -> std::shared_ptr<Context<ResultType>> override {
        return parent_;
    }

    // ========== Result management ==========

    auto set_result(const ResultType& result) -> void override {
        std::unique_lock lock(mutex_);
        result_ = result;
        cv_.notify_all();
    }

    [[nodiscard]] auto get_result(int timeout_ms = -1) const noexcept
        -> std::optional<ResultType> override {
        std::unique_lock lock(mutex_);

        if (timeout_ms < 0) {
            cv_.wait(lock, [this] { return result_.has_value() || done_; });
        } else {
            cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                        [this] { return result_.has_value() || done_; });
        }

        return result_;
    }

private:
    static auto get_current_time_ms() -> uint64_t {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    auto start_timer() -> void {
        timer_thread_ = std::thread([this] {
            auto current = get_current_time_ms();
            if (current < deadline_ms_) {
                std::unique_lock lock(mutex_);
                cv_.wait_until(lock, std::chrono::steady_clock::now() +
                                       std::chrono::milliseconds(deadline_ms_ - current));
            }

            if (!done_) {
                std::unique_lock lock(mutex_);
                if (!done_ && get_current_time_ms() >= deadline_ms_) {
                    done_ = true;
                    error_ = "context deadline exceeded";
                    cv_.notify_all();
                }
            }
        });
    }

    std::shared_ptr<Context<ResultType>> parent_;
    std::string id_;
    uint64_t deadline_ms_;
    std::optional<std::string> error_;
    std::atomic<bool> done_;
    std::optional<ResultType> result_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    std::thread timer_thread_;
};

// ================================
// ValueContext (context carrying values)
// ================================

/**
 * @brief Context carrying key-value pairs
 *
 * Use cases:
 * - Pass metadata
 * - Hierarchical context
 */
template<typename ResultType>
class ValueContext : public Context<ResultType> {
public:
    ValueContext(
        std::shared_ptr<Context<ResultType>> parent,
        std::string_view key,
        glz::json_t value
    )
        : parent_(std::move(parent))
        , id_(utils::UUID::generate_v4())
        , key_(key)
        , value_(std::move(value))
    {}

    // ========== Context interface implementation ==========

    [[nodiscard]] auto get_deadline() const noexcept
        -> std::optional<uint64_t> override {
        return parent_ ? parent_->get_deadline() : std::nullopt;
    }

    [[nodiscard]] auto is_done() const noexcept -> bool override {
        return parent_ ? parent_->is_done() : false;
    }

    [[nodiscard]] auto get_error() const noexcept
        -> std::optional<std::string> override {
        return parent_ ? parent_->get_error() : std::nullopt;
    }

    [[nodiscard]] auto get_value(std::string_view key) const noexcept
        -> std::optional<glz::json_t> override {
        if (key == key_) {
            return value_;
        }
        return parent_ ? parent_->get_value(key) : std::nullopt;
    }

    [[nodiscard]] auto get_id() const noexcept
        -> const std::string& override {
        return id_;
    }

    [[nodiscard]] auto get_parent() const noexcept
        -> std::shared_ptr<Context<ResultType>> override {
        return parent_;
    }

    // ========== Result management (delegate to parent context) ==========

    auto set_result(const ResultType& result) -> void override {
        if (parent_) {
            parent_->set_result(result);
        }
    }

    [[nodiscard]] auto get_result(int timeout_ms = -1) const noexcept
        -> std::optional<ResultType> override {
        return parent_ ? parent_->get_result(timeout_ms) : std::nullopt;
    }

private:
    std::shared_ptr<Context<ResultType>> parent_;
    std::string id_;
    std::string key_;
    glz::json_t value_;
};

// ================================
// Factory functions
// ================================

/**
 * @brief Create empty root context
 */
template<typename ResultType>
[[nodiscard]] auto background() -> std::shared_ptr<Context<ResultType>> {
    static auto instance = std::make_shared<EmptyContext<ResultType>>();
    return instance;
}

/**
 * @brief Create TODO context
 */
template<typename ResultType>
[[nodiscard]] auto todo() -> std::shared_ptr<Context<ResultType>> {
    return background<ResultType>();
}

/**
 * @brief Create cancelable child context
 */
template<typename ResultType>
[[nodiscard]] auto with_cancel(
    std::shared_ptr<Context<ResultType>> parent
) -> std::shared_ptr<Context<ResultType>> {
    return std::make_shared<CancelContext<ResultType>>(parent);
}

/**
 * @brief create child context with timeout
 */
template<typename ResultType>
[[nodiscard]] auto with_timeout(
    std::shared_ptr<Context<ResultType>> parent,
    uint64_t timeout_ms
) -> std::shared_ptr<Context<ResultType>> {
    return std::make_shared<TimeoutContext<ResultType>>(parent, timeout_ms);
}

/**
 * @brief Create child context with deadline
 */
template<typename ResultType>
[[nodiscard]] auto with_deadline(
    std::shared_ptr<Context<ResultType>> parent,
    uint64_t deadline_ms
) -> std::shared_ptr<Context<ResultType>> {
    auto current = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

    uint64_t timeout_ms = (deadline_ms > current) ? (deadline_ms - current) : 0;
    return with_timeout<ResultType>(parent, timeout_ms);
}

/**
 * @brief Create child context carrying value
 */
template<typename ResultType>
[[nodiscard]] auto with_value(
    std::shared_ptr<Context<ResultType>> parent,
    std::string_view key,
    glz::json_t value
) -> std::shared_ptr<Context<ResultType>> {
    return std::make_shared<ValueContext<ResultType>>(
        parent,
        key,
        std::move(value)
    );
}

// ================================
// Type aliases
// ================================

/// Context using model::Result
using ModelContext = Context<model::Result>;

/// Context using A2A Task
// using A2AContext = Context<protocol::a2a::Task>;

/// Context using string
using StringContext = Context<std::string>;

/// Context with no return value
using VoidContext = Context<std::monostate>;

} // namespace moltcat::messaging
