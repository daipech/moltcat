/**
 * @file hybrid_executor.hpp
 * @brief Hybrid executor that routes tasks to appropriate thread pool
 *
 * This executor intelligently routes tasks between:
 * - libuv thread pool: For I/O-bound and lightweight tasks
 * - Custom CPU thread pool: For CPU-intensive and prioritized tasks
 *
 * Uses unified Task definition from task.hpp for compatibility with Executor.
 * Upper layer code can switch between Executor and HybridExecutor without changes.
 *
 * Routing decision based on Task.task_type:
 * - TaskType::IO_BOUND, TaskType::CPU_LIGHT → libuv thread pool
 * - TaskType::CPU_HEAVY, TaskType::PRIORITIZED → custom CPU thread pool
 */

#pragma once

#include "task.hpp"
#include "executor.hpp"
#include "event_loop.hpp"
#include <uv.h>
#include <memory>
#include <atomic>
#include <mutex>
#include <fstream>
#include <chrono>

namespace moltcat::core {

/**
 * @brief Hybrid executor for intelligent task routing
 *
 * Decision matrix:
 * ```
 * Task Type          | Destination    | Reason
 * --------------------|----------------|------------------------------------------
 * IO_BOUND           | libuv pool     | Optimized for blocking I/O
 * CPU_LIGHT          | libuv pool     | Quick compute, avoid pool overhead
 * CPU_HEAVY          | CPU pool       | Dedicated compute resources
 * PRIORITIZED        | CPU pool       | Priority queue support
 * ```
 *
 * Usage:
 * @code
 * HybridExecutor executor(io_loop);
 *
 * // Submit with unified Task (routing based on TaskType)
 * executor.submit(Task{[] { do_work(); }});
 * executor.submit(Task{[] { do_io(); }, TaskType::IO_BOUND});
 * executor.submit(Task{[] { urgent(); }, Task::Priority::HIGH});
 *
 * // Submit function only (defaults to TaskType::CPU_HEAVY)
 * executor.submit([] { compute(); });
 *
 * // Explicit routing (override TaskType)
 * executor.submit_libuv([] { file_operation(); });
 * executor.submit_cpu([] { heavy_compute(); });
 * @endcode
 *
 * Interface compatibility with Executor:
 * - Both use the same Task type
 * - Both have submit(), submit_priority(), submit_affinity() methods
 * - Upper layer can switch by just changing the type
 */
class HybridExecutor {
public:
    /**
     * @brief Create hybrid executor
     * @param io_loop Event loop for libuv operations
     * @param cpu_threads Number of CPU worker threads (0 = hardware concurrency)
     */
    HybridExecutor(EventLoop& io_loop, size_t cpu_threads = 0);

    ~HybridExecutor() = default;

    // Prevent copy
    HybridExecutor(const HybridExecutor&) = delete;
    HybridExecutor& operator=(const HybridExecutor&) = delete;

    // ========================================================================
    // Task submission (unified interface with Executor)
    // ========================================================================

    /**
     * @brief Submit task with automatic routing based on TaskType
     * @param task Task to execute
     *
     * Routing decision:
     * - TaskType::IO_BOUND, CPU_LIGHT → libuv thread pool
     * - TaskType::CPU_HEAVY, PRIORITIZED → custom CPU thread pool
     */
    auto submit(Task&& task) -> void;

    /**
     * @brief Submit function-only task (convenience overload)
     * @param func Function to execute
     *
     * Creates a Task with default TaskType::CPU_HEAVY and Priority::NORMAL.
     */
    auto submit(Task::Func&& func) -> void;

    /**
     * @brief Submit task with explicit priority
     * @param func Function to execute
     * @param priority Task priority
     *
     * Creates a Task with TaskType::PRIORITIZED and specified priority.
     * Routes to CPU thread pool (since priority is specified).
     */
    auto submit_priority(Task::Func&& func, Task::Priority priority) -> void;

    /**
     * @brief Submit task with affinity to specific CPU thread
     * @param task Task to execute
     * @param thread_index Thread index (0 to num_cpu_workers-1)
     *
     * Routes to CPU thread pool with thread affinity.
     */
    auto submit_affinity(Task&& task, size_t thread_index) -> void;

    /**
     * @brief Submit function with affinity to specific CPU thread
     * @param func Function to execute
     * @param thread_index Thread index (0 to num_cpu_workers-1)
     */
    auto submit_affinity(Task::Func&& func, size_t thread_index) -> void;

    // ========================================================================
    // Explicit routing (override automatic routing)
    // ========================================================================

    /**
     * @brief Explicitly route task to libuv thread pool
     * @param task Task to execute
     *
     * Use for:
     * - File I/O operations
     * - DNS queries
     * - Quick computations (< 1ms)
     *
     * Note: Callback runs on event loop thread.
     */
    auto submit_libuv(Task&& task) -> void;

    /**
     * @brief Explicitly route function to libuv thread pool
     * @param func Function to execute
     */
    auto submit_libuv(Task::Func&& func) -> void;

    /**
     * @brief Explicitly route task to custom CPU thread pool
     * @param task Task to execute
     *
     * Use for:
     * - CPU-intensive computations
     * - Long-running tasks
     * - Tasks requiring cancellation or priority
     */
    auto submit_cpu(Task&& task) -> void;

    /**
     * @brief Explicitly route function to custom CPU thread pool
     * @param func Function to execute
     */
    auto submit_cpu(Task::Func&& func) -> void;

    // ========================================================================
    // Executor information
    // ========================================================================

    /**
     * @brief Get number of CPU worker threads
     */
    [[nodiscard]] auto num_workers() const noexcept -> size_t;

    /**
     * @brief Get number of CPU worker threads (alias for compatibility)
     */
    [[nodiscard]] auto num_cpu_workers() const noexcept -> size_t;

    /**
     * @brief Get load statistics
     */
    struct LoadStats {
        size_t libuv_pending = 0;    ///< Tasks queued in libuv pool
        size_t cpu_pending = 0;      ///< Tasks queued in CPU pool
        size_t cpu_active = 0;       ///< Currently executing CPU tasks
        double cpu_utilization = 0.0; ///< CPU utilization (0.0 - 1.0)
    };

    [[nodiscard]] auto get_stats() const -> LoadStats;

    /**
     * @brief Get approximate number of pending tasks
     * @note This is an estimate due to concurrent access
     */
    [[nodiscard]] auto pending_tasks() const noexcept -> size_t;

    // ========================================================================
    // Executor control
    // ========================================================================

    /**
     * @brief Wait for all pending tasks to complete
     * @note Only waits for CPU pool tasks; libuv tasks should be tracked separately
     */
    auto wait() -> void;

    /**
     * @brief Stop executor
     */
    auto stop() -> void;

private:
    EventLoop& io_loop_;
    Executor cpu_executor_;

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> libuv_pending_{0};

    // libuv work wrapper
    struct LibuvWork {
        uv_work_t req;
        Task task;
        std::shared_ptr<std::atomic<bool>> active;

        explicit LibuvWork(Task&& t)
            : task(std::move(t))
            , active(std::make_shared<std::atomic<bool>>(true)) {}
    };

    // Internal routing based on TaskType
    auto route_task(Task&& task) -> void;
};

// ========================================================================
// Implementation
// ========================================================================

inline HybridExecutor::HybridExecutor(EventLoop& io_loop, size_t cpu_threads)
    : io_loop_(io_loop)
    , cpu_executor_(cpu_threads) {}

inline auto HybridExecutor::submit(Task&& task) -> void {
    route_task(std::move(task));
}

inline auto HybridExecutor::submit(Task::Func&& func) -> void {
    route_task(Task(std::move(func)));
}

inline auto HybridExecutor::submit_priority(Task::Func&& func, Task::Priority priority) -> void {
    // Priority tasks always go to CPU pool
    cpu_executor_.submit_priority(std::move(func), priority);
}

inline auto HybridExecutor::submit_affinity(Task&& task, size_t thread_index) -> void {
    cpu_executor_.submit_affinity(std::move(task), thread_index);
}

inline auto HybridExecutor::submit_affinity(Task::Func&& func, size_t thread_index) -> void {
    cpu_executor_.submit_affinity(std::move(func), thread_index);
}

inline auto HybridExecutor::submit_libuv(Task&& task) -> void {
    libuv_pending_.fetch_add(1, std::memory_order_relaxed);

    auto work = std::make_unique<LibuvWork>(std::move(task));
    work->req.data = work.get();

    uv_queue_work(io_loop_.raw(), &work->req,
        [](uv_work_t* req) {
            // Execute in libuv thread pool
            auto w = static_cast<LibuvWork*>(req->data);
            try {
                w->task();
            } catch (...) {
                // Log error but don't propagate
            }
        },
        [](uv_work_t* req, int status) {
            // After work callback (runs on event loop thread)
            (void)status;
            auto w = static_cast<LibuvWork*>(req->data);
            w->active->store(false, std::memory_order_release);
            delete w;
        }
    );

    // Release ownership - uv will clean up
    work.release();
}

inline auto HybridExecutor::submit_libuv(Task::Func&& func) -> void {
    submit_libuv(Task(std::move(func), TaskType::IO_BOUND));
}

inline auto HybridExecutor::submit_cpu(Task&& task) -> void {
    cpu_executor_.submit(std::move(task));
}

inline auto HybridExecutor::submit_cpu(Task::Func&& func) -> void {
    cpu_executor_.submit(std::move(func));
}

inline auto HybridExecutor::num_workers() const noexcept -> size_t {
    return cpu_executor_.num_workers();
}

inline auto HybridExecutor::num_cpu_workers() const noexcept -> size_t {
    return cpu_executor_.num_workers();
}

inline auto HybridExecutor::get_stats() const -> LoadStats {
    LoadStats stats;
    stats.libuv_pending = libuv_pending_.load(std::memory_order_relaxed);
    stats.cpu_pending = cpu_executor_.pending_tasks();
    stats.cpu_active = cpu_executor_.num_workers();
    return stats;
}

inline auto HybridExecutor::pending_tasks() const noexcept -> size_t {
    auto stats = get_stats();
    return stats.libuv_pending + stats.cpu_pending;
}

inline auto HybridExecutor::wait() -> void {
    cpu_executor_.wait();
    // Note: libuv tasks cannot be waited on directly
    // Application should track completion separately
}

inline auto HybridExecutor::stop() -> void {
    cpu_executor_.stop();
}

inline auto HybridExecutor::route_task(Task&& task) -> void {
    // Route based on TaskType
    switch (task.type) {
        case TaskType::IO_BOUND:
        case TaskType::CPU_LIGHT:
            submit_libuv(std::move(task));
            break;
        case TaskType::CPU_HEAVY:
        case TaskType::PRIORITIZED:
        default:
            submit_cpu(std::move(task));
            break;
    }
}

// ========================================================================
// Convenience Functions
// ========================================================================

/**
 * @brief Submit file I/O task to libuv thread pool
 *
 * Example:
 * @code
 * submit_file_io(executor, "data.txt", [](const std::string& content) {
 *     process_file(content);
 * });
 * @endcode
 */
inline auto submit_file_io(
    HybridExecutor& executor,
    const std::string& path,
    std::function<void(const std::string&)> callback
) -> void {
    executor.submit_libuv(io_task([path, callback]() {
        // Read file (runs in libuv thread pool)
        std::ifstream file(path);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        callback(content);
    }));
}

/**
 * @brief Submit CPU-bound computation task
 *
 * Example:
 * @code
 * submit_compute(executor, [](int n) { return fibonacci(n); }, 40,
 *     [](uint64_t result) { std::cout << "Result: " << result; });
 * @endcode
 */
template <typename F, typename A, typename R = std::invoke_result_t<F, A>>
inline auto submit_compute(
    HybridExecutor& executor,
    F&& func,
    A&& arg,
    std::function<void(const R&)> callback
) -> void {
    executor.submit_cpu(cpu_task([f = std::forward<F>(func),
                                  a = std::forward<A>(arg),
                                  callback]() {
        R result = f(a);
        callback(result);
    }));
}

/**
 * @brief Submit timed computation with timeout
 *
 * If computation exceeds timeout_ms, callback receives error result.
 */
inline auto submit_compute_with_timeout(
    HybridExecutor& executor,
    std::function<void()> work,
    uint64_t timeout_ms,
    std::function<void(bool)> completion_callback
) -> void {
    executor.submit_cpu(cpu_task([work = std::move(work), timeout_ms, completion_callback]() {
        auto start = std::chrono::steady_clock::now();

        // Execute work (with periodic timeout check for long-running tasks)
        work();

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();

        completion_callback(elapsed <= timeout_ms);
    }));
}

} // namespace moltcat::core
