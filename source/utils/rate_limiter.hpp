#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <cstdint>

namespace moltcat::utils {

/**
 * RateLimiter - Sliding window rate limiter
 *
 * Implements precise rate limiting using sliding time window algorithm.
 *
 * Features:
 * - High precision: timestamp-based sliding window
 * - Thread-safe: uses mutex to protect internal state
 * - Flexible configuration: customizable window size and request count
 *
 * Use cases:
 * - LLM API call rate limiting
 * - Message sending rate limiting
 * - Network request rate limiting
 *
 * Example:
 * ```cpp
 * RateLimiter limiter(60, 1s);  // Max 60 requests per second
 *
 * if (limiter.allow_request()) {
 *     // Execute request
 * } else {
 *     // Request rate limited
 * }
 * ```
 */
class RateLimiter {
public:
    /**
     * Timestamp type
     */
    using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;
    using Duration = std::chrono::milliseconds;

    /**
     * Constructor
     *
     * @param max_requests Maximum number of requests allowed within time window
     * @param window_size Time window size (milliseconds)
     */
    RateLimiter(uint32_t max_requests, uint64_t window_size_ms)
        : max_requests_(max_requests)
        , window_size_(std::chrono::milliseconds(window_size_ms))
    {}

    /**
     * Constructor (using Duration)
     *
     * @param max_requests Maximum number of requests allowed within time window
     * @param window_size Time window size
     */
    RateLimiter(uint32_t max_requests, Duration window_size)
        : max_requests_(max_requests)
        , window_size_(window_size)
    {}

    ~RateLimiter() = default;

    // Disable copy and move
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;
    RateLimiter(RateLimiter&&) = delete;
    RateLimiter& operator=(RateLimiter&&) = delete;

    /**
     * Check if request is allowed
     *
     * If allowed, records the current request timestamp.
     * If not allowed, does not record timestamp.
     *
     * @return true means request is allowed, false means rate limited
     *
     * @note Thread-safe
     */
    auto allow_request() -> bool;

    /**
     * Check if request is allowed (without recording)
     *
     * Check only, does not record timestamp (for pre-check).
     *
     * @return true means request is allowed, false means rate limited
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto check() const -> bool;

    /**
     * Reset rate limiter
     *
     * Clears all timestamp records.
     *
     * @note Thread-safe
     */
    auto reset() -> void;

    /**
     * Get request count in current window
     *
     * @return Number of requests in current window
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto get_current_count() const -> uint32_t;

    /**
     * Get window size
     */
    [[nodiscard]] auto get_window_size() const noexcept -> Duration {
        return window_size_;
    }

    /**
     * Get maximum request count
     */
    [[nodiscard]] auto get_max_requests() const noexcept -> uint32_t {
        return max_requests_;
    }

    /**
     * Get wait time for next allowed request
     *
     * @return Wait time (milliseconds), 0 means can request immediately
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto get_wait_time_ms() const -> uint64_t;

private:
    /**
     * Clean up expired timestamps
     */
    auto cleanup_expired_timestamps() const -> void;

    // ================================
    // Fields
    // ================================

    uint32_t max_requests_;                           // Maximum requests allowed within time window
    Duration window_size_;                             // Time window size

    // Timestamp records (mutable, for cleanup)
    mutable std::mutex mutex_;
    mutable std::deque<Timestamp> timestamps_;         // Request timestamp queue
};

/**
 * TokenBucket - Token bucket rate limiter
 *
 * Token bucket algorithm: adds tokens to bucket at fixed rate, requests consume tokens.
 *
 * Features:
 * - Allows burst traffic: can process multiple requests at once when bucket is full
 * - Smooth rate limiting: stable long-term average rate
 * - Thread-safe
 *
 * Use cases:
 * - API calls with burst allowance
 * - Network traffic shaping
 * - Message queue rate limiting
 */
class TokenBucket {
public:
    /**
     * Constructor
     *
     * @param capacity Bucket capacity (maximum token count)
     * @param refill_rate Token refill rate (tokens added per second)
     */
    TokenBucket(uint32_t capacity, double refill_rate)
        : capacity_(capacity)
        , refill_rate_(refill_rate)
        , tokens_(static_cast<double>(capacity))
        , last_refill_(std::chrono::steady_clock::now())
    {}

    ~TokenBucket() = default;

    // Disable copy and move
    TokenBucket(const TokenBucket&) = delete;
    TokenBucket& operator=(const TokenBucket&) = delete;
    TokenBucket(TokenBucket&&) = delete;
    TokenBucket& operator=(TokenBucket&&) = delete;

    /**
     * Try to consume tokens
     *
     * @param tokens Number of tokens to consume (default 1)
     * @return true means successfully consumed tokens, false means insufficient tokens
     *
     * @note Thread-safe
     */
    auto try_consume(uint32_t tokens = 1) -> bool;

    /**
     * Check if there are enough tokens
     *
     * @param tokens Number of tokens needed (default 1)
     * @return true means sufficient tokens
     *
     * @note Thread-safe
     */
    [[nodiscard]] auto check(uint32_t tokens = 1) const -> bool;

    /**
     * Reset token bucket
     *
     * Resets token count to capacity.
     *
     * @note Thread-safe
     */
    auto reset() -> void;

    /**
     * Get current token count
     */
    [[nodiscard]] auto get_current_tokens() const -> double;

    /**
     * Get bucket capacity
     */
    [[nodiscard]] auto get_capacity() const noexcept -> uint32_t {
        return capacity_;
    }

    /**
     * Get refill rate
     */
    [[nodiscard]] auto get_refill_rate() const noexcept -> double {
        return refill_rate_;
    }

private:
    /**
     * Refill tokens (based on time difference)
     */
    auto refill() -> void;

    // ================================
    // Fields
    // ================================

    uint32_t capacity_;                              // Bucket capacity
    double refill_rate_;                             // Token refill rate (tokens/second)
    double tokens_;                                  // Current token count

    std::chrono::time_point<std::chrono::steady_clock> last_refill_;  // Last refill time
    mutable std::mutex mutex_;                       // Thread safety
};

} // namespace moltcat::utils
