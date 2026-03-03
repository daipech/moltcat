#include "utils/rate_limiter.hpp"

namespace moltcat::utils {

// ================================
// RateLimiter implementation
// ================================

auto RateLimiter::allow_request() -> bool {
    std::lock_guard lock(mutex_);

    // Clean up expired timestamps
    cleanup_expired_timestamps();

    // Check if limit exceeded
    if (timestamps_.size() >= static_cast<size_t>(max_requests_)) {
        return false;
    }

    // Record current request timestamp
    timestamps_.push_back(std::chrono::steady_clock::now());
    return true;
}

auto RateLimiter::check() const -> bool {
    std::lock_guard lock(mutex_);

    // Clean up expired timestamps
    cleanup_expired_timestamps();

    // Check if limit exceeded
    return timestamps_.size() < static_cast<size_t>(max_requests_);
}

auto RateLimiter::reset() -> void {
    std::lock_guard lock(mutex_);
    timestamps_.clear();
}

auto RateLimiter::get_current_count() const -> uint32_t {
    std::lock_guard lock(mutex_);

    // Clean up expired timestamps
    cleanup_expired_timestamps();

    return static_cast<uint32_t>(timestamps_.size());
}

auto RateLimiter::get_wait_time_ms() const -> uint64_t {
    std::lock_guard lock(mutex_);

    // Clean up expired timestamps
    cleanup_expired_timestamps();

    // If not exceeded limit, no wait needed
    if (timestamps_.size() < static_cast<size_t>(max_requests_)) {
        return 0;
    }

    // Calculate when oldest request expires
    if (timestamps_.empty()) {
        return 0;
    }

    auto now = std::chrono::steady_clock::now();
    auto oldest = timestamps_.front();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - oldest);

    if (elapsed >= window_size_) {
        return 0;
    }

    return static_cast<uint64_t>((window_size_ - elapsed).count());
}

auto RateLimiter::cleanup_expired_timestamps() const -> void {
    auto now = std::chrono::steady_clock::now();

    // Remove all timestamps outside window
    while (!timestamps_.empty()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - timestamps_.front()
        );

        if (elapsed >= window_size_) {
            timestamps_.pop_front();
        } else {
            break;
        }
    }
}

// ================================
// TokenBucket implementation
// ================================

auto TokenBucket::try_consume(uint32_t tokens) -> bool {
    std::lock_guard lock(mutex_);

    // Refill tokens
    refill();

    // Check if sufficient tokens
    if (tokens_ >= static_cast<double>(tokens)) {
        tokens_ -= static_cast<double>(tokens);
        return true;
    }

    return false;
}

auto TokenBucket::check(uint32_t tokens) const -> bool {
    std::lock_guard lock(mutex_);

    // Refill tokens (const_cast because refill is not const)
    const_cast<TokenBucket*>(this)->refill();

    // Check if sufficient tokens
    return tokens_ >= static_cast<double>(tokens);
}

auto TokenBucket::reset() -> void {
    std::lock_guard lock(mutex_);
    tokens_ = static_cast<double>(capacity_);
    last_refill_ = std::chrono::steady_clock::now();
}

auto TokenBucket::get_current_tokens() const -> double {
    std::lock_guard lock(mutex_);

    // Refill tokens
    const_cast<TokenBucket*>(this)->refill();

    return tokens_;
}

auto TokenBucket::refill() -> void {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_refill_
    );

    // Calculate tokens to add
    double elapsed_seconds = elapsed.count() / 1000.0;
    double tokens_to_add = refill_rate_ * elapsed_seconds;

    // Refill tokens (not exceeding capacity)
    tokens_ = std::min(tokens_ + tokens_to_add, static_cast<double>(capacity_));

    // Update last refill time
    last_refill_ = now;
}

} // namespace moltcat::utils
