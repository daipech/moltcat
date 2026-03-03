/**
 * @file executor.hpp
 * @brief Work-stealing thread pool for CPU-intensive tasks
 *
 * Features:
 * - Work stealing algorithm for load balancing
 * - Priority queue support
 * - C++20 coroutine integration
 * - Thread affinity control
 *
 * Uses unified Task definition from task.hpp for compatibility with HybridExecutor.
 */

#pragma once

#include "task.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <coroutine>

namespace moltcat::core {

/**
 * @brief Work-stealing thread pool
 *
 * Architecture:
 * ```
 * Global Queue (Priority)     Worker 1    Worker 2    Worker 3
 *     [MPSC]                    [LQ]        [LQ]        [LQ]
 *        |                         |           |           |
 *        |                         v           v           v
 *        |                      [Steal] <---> [Steal] <---> [Steal]
 *        v
 *     Workers can steal from each other's local queues
 * ```
 *
 * Usage:
 * @code
 * Executor executor(4);  // 4 worker threads
 *
 * // Submit with unified Task
 * executor.submit(Task{[] { do_work(); }});
 *
 * // Submit task with priority
 * executor.submit(Task{[] { urgent_work(); }, Task::Priority::HIGH});
 *
 * // Submit task with type hint (ignored by Executor, used for compatibility)
 * executor.submit(Task{[] { compute(); }, TaskType::CPU_HEAVY});
 *
 * // Submit function only (convenience)
 * executor.submit([] { quick_task(); });
 *
 * // Submit with return value
 * auto future = executor.submit_future([] -> int { return compute(); });
 * int result = future.get();
 * @endcode
 *
 * Note: TaskType field is ignored by Executor. All tasks are routed to worker threads.
 * Use HybridExecutor if you need TaskType-based routing between libuv and CPU pools.
 */
class Executor {
public:
    /**
     * @brief Create executor with specified number of threads
     * @param num_threads Number of worker threads (0 = hardware concurrency)
     */
    explicit Executor(size_t num_threads = 0);
    ~Executor();

    // Prevent copy
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

    // ========================================================================
    // Task submission
    // ========================================================================

    /**
     * @brief Submit task to executor
     * @param task Task to execute (unified Task type)
     *
     * Task is queued with its priority and executed on an available worker thread.
     * TaskType field is ignored - all tasks go to worker threads.
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
     */
    auto submit_priority(Task::Func&& func, Task::Priority priority) -> void;

    /**
     * @brief Submit task with return value
     * @param func Function to execute
     * @return Future for the result
     */
    template <typename F, typename R = std::invoke_result_t<F>>
    [[nodiscard]] auto submit_future(F&& func) -> std::future<R> {
        auto promise = std::make_shared<std::promise<R>>();
        auto future = promise->get_future();

        submit(Task([p = std::move(promise), f = std::forward<F>(func)]() mutable {
            try {
                if constexpr (std::is_void_v<R>) {
                    f();
                    p->set_value();
                } else {
                    p->set_value(f());
                }
            } catch (...) {
                p->set_exception(std::current_exception());
            }
        }, TaskType::PRIORITIZED));

        return future;
    }

    /**
     * @brief Submit task with affinity to specific thread
     * @param task Task to execute
     * @param thread_index Thread index (0 to num_workers-1)
     *
     * Task will only execute on the specified thread.
     */
    auto submit_affinity(Task&& task, size_t thread_index) -> void;

    /**
     * @brief Submit function with affinity to specific thread
     * @param func Function to execute
     * @param thread_index Thread index (0 to num_workers-1)
     */
    auto submit_affinity(Task::Func&& func, size_t thread_index) -> void;

    // ========================================================================
    // Executor control
    // ========================================================================

    /**
     * @brief Get number of worker threads
     */
    [[nodiscard]] auto num_workers() const noexcept -> size_t;

    /**
     * @brief Get approximate number of pending tasks
     * @note This is an estimate due to concurrent access
     */
    [[nodiscard]] auto pending_tasks() const noexcept -> size_t;

    /**
     * @brief Wait for all pending tasks to complete
     * @note Does NOT stop new tasks from being submitted
     */
    auto wait() -> void;

    /**
     * @brief Stop executor and wait for workers to finish
     * @note Prevents new tasks from being submitted
     */
    auto stop() -> void;

private:
    class Impl;
    class Worker;

    std::unique_ptr<Impl> impl_;
    std::vector<std::unique_ptr<Worker>> workers_;

    // Internal helpers (called by Worker)
    friend class Worker;
    auto try_get_global_task(Task& task) -> bool;
    auto try_steal_from(size_t victim_index, Task& task) -> bool;
    auto notify_task_completed() -> void;
};

// ========================================================================
// Coroutine Support (C++20)
// ========================================================================

namespace detail {

/**
 * @brief Awaitable that submits task to executor
 */
struct ExecutorAwaitable {
    Executor& executor;
    Task::Func func;

    auto await_ready() const noexcept -> bool { return false; }

    auto await_suspend(std::coroutine_handle<> handle) -> void {
        // Submit the task with continuation
        executor.submit(Task([this, handle]() mutable {
            func();
            handle.resume();
        }));
    }

    auto await_resume() const noexcept -> void {}
};

} // namespace detail

/**
 * @brief Submit coroutine task to executor
 *
 * Usage in coroutine:
 * @code
 * auto my_coroutine() -> std::future<void> {
 *     // This runs on caller's thread
 *     prepare_data();
 *
 *     // Switch to executor thread
 *     co_await schedule_on(executor);
 *
 *     // This runs on executor thread
 *     do_heavy_work();
 * }
 * @endcode
 *
 * @param executor Executor to schedule on
 * @return Awaitable for co_await
 */
[[nodiscard]] inline auto schedule_on(Executor& executor) -> detail::ExecutorAwaitable {
    return {executor, Task::Func{}};
}

/**
 * @brief Submit function to executor as coroutine awaitable
 */
[[nodiscard]] inline auto schedule_on(Executor& executor, Task::Func func)
    -> detail::ExecutorAwaitable {
    return {executor, std::move(func)};
}

} // namespace moltcat::core
