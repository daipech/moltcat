/**
 * @file event_loop.hpp
 * @brief libuv event loop wrapper
 *
 * Each thread that needs I/O should have its own event loop.
 * Use uv_async_send() for cross-thread communication.
 */

#pragma once

#include <uv.h>
#include <memory>
#include <functional>
#include <stdexcept>

namespace moltcat::core {

/**
 * @brief libuv event loop wrapper
 *
 * Usage:
 * @code
 * // Create and run in a dedicated thread
 * EventLoop loop;
 * loop.run();  // Blocks until loop.stop() is called
 * @endcode
 */
class EventLoop {
public:
    /**
     * @brief Async callback type
     * @param data User data passed to callback
     */
    using AsyncCallback = std::function<void(void* data)>;

    /**
     * @brief Timer callback type
     * @param data User data passed to callback
     */
    using TimerCallback = std::function<void(void* data)>;

    EventLoop();
    ~EventLoop();

    // Prevent copy
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // Allow move
    EventLoop(EventLoop&& other) noexcept;
    EventLoop& operator=(EventLoop&& other) noexcept;

    /**
     * @brief Get raw libuv loop
     * @return Pointer to uv_loop_t
     */
    [[nodiscard]] auto raw() noexcept -> uv_loop_t* { return loop_.get(); }

    /**
     * @brief Get raw libuv loop (const)
     * @return Const pointer to uv_loop_t
     */
    [[nodiscard]] auto raw() const noexcept -> const uv_loop_t* { return loop_.get(); }

    /**
     * @brief Run the event loop
     * @param mode Run mode (default: UV_RUN_DEFAULT)
     * @return 0 on success, error code on failure
     *
     * UV_RUN_DEFAULT: Run until there are no active handles
     * UV_RUN_ONCE: Run for one iteration
     * UV_RUN_NOWAIT: Run for one iteration in non-blocking mode
     */
    auto run(uv_run_mode mode = UV_RUN_DEFAULT) -> int;

    /**
     * @brief Stop the event loop
     * @note Thread-safe, can be called from any thread
     */
    auto stop() -> void;

    /**
     * @brief Get loop alive status
     * @return true if loop is running
     */
    [[nodiscard]] auto is_alive() const -> bool;

    /**
     * @brief Get loop time
     * @return Current loop time in milliseconds
     */
    [[nodiscard]] auto now() const -> uint64_t;

    /**
     * @brief Update loop time
     */
    auto update_time() -> void;

    /**
     * @brief Create async handle for cross-thread communication
     * @param callback Callback to invoke when async is sent
     * @param data User data to pass to callback
     * @return Unique pointer to async handle
     *
     * Usage:
     * @code
     * auto async = loop.create_async([](void* data) {
     *     // This runs on the event loop thread
     *     auto msg = static_cast<Message*>(data);
     *     process_message(msg);
     * });
     *
     * // From another thread:
     * async->send(data);
     * @endcode
     */
    [[nodiscard]] auto create_async(AsyncCallback callback, void* data = nullptr)
        -> std::unique_ptr<class AsyncHandle>;

    /**
     * @brief Create timer
     * @param callback Callback to invoke when timer expires
     * @param data User data to pass to callback
     * @return Unique pointer to timer handle
     */
    [[nodiscard]] auto create_timer(TimerCallback callback, void* data = nullptr)
        -> std::unique_ptr<class TimerHandle>;

private:
    struct Deleter {
        auto operator()(uv_loop_t* loop) const noexcept -> void;
    };

    std::unique_ptr<uv_loop_t, Deleter> loop_;
};

// ========================================================================
// Async Handle
// ========================================================================

/**
 * @brief Async handle for cross-thread communication
 *
 * Thread-safe: Can send from any thread, callback runs on event loop thread.
 */
class AsyncHandle {
public:
    explicit AsyncHandle(EventLoop& loop, EventLoop::AsyncCallback callback, void* data = nullptr);
    ~AsyncHandle();

    // Prevent copy
    AsyncHandle(const AsyncHandle&) = delete;
    AsyncHandle& operator=(const AsyncHandle&) = delete;

    /**
     * @brief Send async signal
     * @param data Optional data to pass (overrides callback data)
     * @note Thread-safe, can be called from any thread
     */
    auto send(void* data = nullptr) -> void;

    /**
     * @brief Get underlying uv_async_t
     */
    [[nodiscard]] auto raw() noexcept -> uv_async_t* { return &async_; }

private:
    uv_async_t async_;
    EventLoop::AsyncCallback callback_;
    void* user_data_;
};

// ========================================================================
// Timer Handle
// ========================================================================

/**
 * @brief Timer handle
 */
class TimerHandle {
public:
    explicit TimerHandle(EventLoop& loop, EventLoop::TimerCallback callback, void* data = nullptr);
    ~TimerHandle();

    // Prevent copy
    TimerHandle(const TimerHandle&) = delete;
    TimerHandle& operator=(const TimerHandle&) = delete;

    /**
     * @brief Start timer
     * @param timeout_ms Time before first callback (milliseconds)
     * @param repeat_ms Repeat interval (0 = one-shot)
     */
    auto start(uint64_t timeout_ms, uint64_t repeat_ms = 0) -> void;

    /**
     * @brief Stop timer
     */
    auto stop() -> void;

    /**
     * @brief Get underlying uv_timer_t
     */
    [[nodiscard]] auto raw() noexcept -> uv_timer_t* { return &timer_; }

private:
    uv_timer_t timer_;
    EventLoop::TimerCallback callback_;
    void* user_data_;
    EventLoop& loop_;
};

} // namespace moltcat::core
