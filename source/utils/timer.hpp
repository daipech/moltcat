#pragma once

#include <chrono>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <format>

namespace moltcat::utils {

/**
 * @brief High-resolution timer
 *
 * Used to measure code execution time
 */
class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    Timer() = default;

    /**
     * @brief Start timer
     */
    auto start() -> void {
        start_time_ = Clock::now();
        running_ = true;
    }

    /**
     * @brief Stop timer
     */
    auto stop() -> void {
        end_time_ = Clock::now();
        running_ = false;
    }

    /**
     * @brief Reset timer
     */
    auto reset() -> void {
        start_time_ = TimePoint{};
        end_time_ = TimePoint{};
        running_ = false;
    }

    /**
     * @brief Get elapsed time (milliseconds)
     */
    [[nodiscard]] auto elapsed_ms() const -> double {
        auto end = running_ ? Clock::now() : end_time_;
        return std::chrono::duration<double, std::milli>(end - start_time_).count();
    }

    /**
     * @brief Get elapsed time (microseconds)
     */
    [[nodiscard]] auto elapsed_us() const -> double {
        auto end = running_ ? Clock::now() : end_time_;
        return std::chrono::duration<double, std::micro>(end - start_time_).count();
    }

    /**
     * @brief Get elapsed time (nanoseconds)
     */
    [[nodiscard]] auto elapsed_ns() const -> uint64_t {
        auto end = running_ ? Clock::now() : end_time_;
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_time_).count();
    }

    /**
     * @brief Get elapsed time (seconds)
     */
    [[nodiscard]] auto elapsed_s() const -> double {
        auto end = running_ ? Clock::now() : end_time_;
        return std::chrono::duration<double>(end - start_time_).count();
    }

    /**
     * @brief Format elapsed time string
     */
    [[nodiscard]] auto elapsed_str() const -> std::string {
        auto ms = elapsed_ms();
        if (ms < 1.0) {
            return std::format("{:.2f}μs", elapsed_us());
        } else if (ms < 1000.0) {
            return std::format("{:.2f}ms", ms);
        } else {
            return std::format("{:.2f}s", elapsed_s());
        }
    }

    /**
     * @brief Check if timer is running
     */
    [[nodiscard]] auto is_running() const -> bool {
        return running_;
    }

private:
    TimePoint start_time_;
    TimePoint end_time_;
    bool running_{false};
};

/**
 * @brief Scoped timer
 *
 * Automatically outputs execution time on destruction
 */
class ScopedTimer {
public:
    using Callback = std::function<void(const std::string&, double)>;

    explicit ScopedTimer(std::string name)
        : name_(std::move(name))
        , callback_(nullptr) {
        timer_.start();
    }

    ScopedTimer(std::string name, Callback callback)
        : name_(std::move(name))
        , callback_(std::move(callback)) {
        timer_.start();
    }

    ~ScopedTimer() {
        timer_.stop();
        auto ms = timer_.elapsed_ms();

        if (callback_) {
            callback_(name_, ms);
        } else {
            // Default output to log
            // MOLT_LOGGER.info("{} took {}", name_, timer_.elapsed_str());
            // Temporarily no output here to avoid circular dependency
        }
    }

    // Disable copy
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    std::string name_;
    Timer timer_;
    Callback callback_;
};

/**
 * @brief Scheduled timer
 *
 * Used for delayed or periodic task execution
 */
class ScheduledTimer {
public:
    using Callback = std::function<void()>;

    ScheduledTimer() = default;
    ~ScheduledTimer() {
        stop();
    }

    /**
     * @brief Start one-shot timer
     *
     * @param delay_ms Delay time (milliseconds)
     * @param callback Callback function
     */
    auto start_once(uint64_t delay_ms, Callback callback) -> void {
        stop();
        running_ = true;
        thread_ = std::thread([this, delay_ms, callback]() {
            std::unique_lock lock(mutex_);
            if (cv_.wait_for(lock, std::chrono::milliseconds(delay_ms),
                             [this] { return !running_; })) {
                // Timeout, execute callback
                if (running_) {
                    callback();
                }
            }
        });
    }

    /**
     * @brief Start periodic timer
     *
     * @param interval_ms Interval time (milliseconds)
     * @param callback Callback function
     */
    auto start_periodic(uint64_t interval_ms, Callback callback) -> void {
        stop();
        running_ = true;
        thread_ = std::thread([this, interval_ms, callback]() {
            while (true) {
                std::unique_lock lock(mutex_);
                if (cv_.wait_for(lock, std::chrono::milliseconds(interval_ms),
                                 [this] { return !running_; })) {
                    // Timeout, execute callback
                    if (running_) {
                        callback();
                    }
                } else {
                    // Notified to stop
                    break;
                }
            }
        });
    }

    /**
     * @brief Stop timer
     */
    auto stop() -> void {
        {
            std::lock_guard lock(mutex_);
            running_ = false;
            cv_.notify_all();
        }

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    /**
     * @brief Check if running
     */
    [[nodiscard]] auto is_running() const -> bool {
        return running_;
    }

private:
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
};

/**
 * @brief Time utility class
 */
class TimeUtils {
public:
    /**
     * @brief Get current timestamp (milliseconds)
     */
    [[nodiscard]] static auto now_ms() -> uint64_t {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /**
     * @brief Get current timestamp (seconds)
     */
    [[nodiscard]] static auto now_s() -> uint64_t {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /**
     * @brief Get current timestamp (microseconds)
     */
    [[nodiscard]] static auto now_us() -> uint64_t {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /**
     * @brief Convert timestamp to datetime string
     */
    [[nodiscard]] static auto timestamp_to_datetime(uint64_t timestamp_ms,
                                                     bool include_ms = false) -> std::string {
        auto ms = std::chrono::milliseconds(timestamp_ms);
        auto tp = std::chrono::system_clock::time_point(ms);
        auto dp = std::chrono::floor<std::chrono::days>(tp);

        std::string format = include_ms ? "%Y-%m-%d %H:%M:%S." : "%Y-%m-%d %H:%M:%S";
        auto sd = std::chrono::year_month_day{dp};

        std::ostringstream oss;
        oss << std::setfill('0');

        // Date part
        oss << std::setw(4) << static_cast<int>(sd.year()) << '-'
            << std::setw(2) << static_cast<unsigned>(sd.month()) << '-'
            << std::setw(2) << static_cast<unsigned>(sd.day()) << ' ';

        // Time part
        auto time = std::chrono::hh_mm_ss{tp - dp};
        oss << std::setw(2) << time.hours().count() << ':'
            << std::setw(2) << time.minutes().count() << ':'
            << std::setw(2) << time.seconds().count();

        // Milliseconds part
        if (include_ms) {
            auto ms_part = std::chrono::duration_cast<std::chrono::milliseconds>(
                tp - dp - std::chrono::duration_cast<std::chrono::seconds>(time));
            oss << '.' << std::setw(3) << ms_part.count();
        }

        return oss.str();
    }

    /**
     * @brief Format duration
     */
    [[nodiscard]] static auto format_duration(uint64_t milliseconds) -> std::string {
        auto ms = milliseconds;
        auto seconds = ms / 1000;
        auto minutes = seconds / 60;
        auto hours = minutes / 60;
        auto days = hours / 24;

        if (days > 0) {
            return std::format("{}d {}h {}m", days, hours % 24, minutes % 60);
        } else if (hours > 0) {
            return std::format("{}h {}m", hours, minutes % 60);
        } else if (minutes > 0) {
            return std::format("{}m {}s", minutes, seconds % 60);
        } else if (seconds > 0) {
            return std::format("{}s", seconds);
        } else {
            return std::format("{}ms", ms);
        }
    }

    /**
     * @brief Sleep for specified milliseconds
     */
    static auto sleep_ms(uint64_t milliseconds) -> void {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

    /**
     * @brief Sleep for specified microseconds
     */
    static auto sleep_us(uint64_t microseconds) -> void {
        std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
    }
};

/**
 * @brief Convenience macro: scoped timing
 */
#define SCOPED_TIMER(name) moltcat::utils::ScopedTimer _timer_##__LINE__(name)

/**
 * @brief Convenience macro: function timing (outputs execution time when function exits)
 */
#define FUNCTION_TIMER() moltcat::utils::ScopedTimer _func_timer(__FUNCTION__)

} // namespace moltcat::utils
