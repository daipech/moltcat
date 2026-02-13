/**
 * @file scheduler.hpp
 * @brief Task scheduler for coordinating I/O and CPU work
 *
 * The scheduler bridges the gap between I/O threads (libuv event loops)
 * and worker threads. It handles:
 * - Task queuing and prioritization
 * - Timeout management
 * - Result callback routing
 * - Cross-thread communication
 *
 * Design:
 * - IScheduler: Abstract interface for polymorphic use
 * - Scheduler<ExecutorType>: Template implementation for type-specific executors
 * - AnyScheduler: Type-erased wrapper for runtime polymorphism
 */

#pragma once

#include "moltcat/molt_model.hpp"
#include "event_loop.hpp"
#include "task.hpp"
#include "executor.hpp"
#include "hybrid_executor.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <future>
#include <coroutine>

namespace moltcat::core {

// ========================================================================
// Forward Declarations
// ========================================================================

template <typename ExecutorType>
class Scheduler;

class AnyScheduler;

// ========================================================================
// Scheduled Task (Shared across all scheduler types)
// ========================================================================

/**
 * @brief Task execution context
 *
 * Contains all information needed to execute a task,
 * including the task itself, callback, and timeout tracking.
 */
class ScheduledTask {
public:
    using Callback = std::function<void(const MoltResult&)>;

    ScheduledTask() = default;
    ScheduledTask(
        const MoltTask& t,
        Callback cb,
        uint64_t timeout_ms = 0
    );

    MoltTask task;
    Callback callback;
    uint64_t timeout_ms = 0;
    uint64_t deadline = 0;  // Absolute time in milliseconds
    bool completed = false;
    bool timed_out = false;
};

// ========================================================================
// Abstract Scheduler Interface
// ========================================================================

/**
 * @brief Abstract scheduler interface
 *
 * This interface defines the contract for all scheduler implementations.
 * Use this when you need runtime polymorphism or dependency injection.
 *
 * Usage:
 * @code
 * void process_tasks(IScheduler& scheduler) {
 *     scheduler.schedule(task, [](const MoltResult& r) {
 *         // Handle result
 *     });
 * }
 * @endcode
 */
class IScheduler {
public:
    using Callback = std::function<void(const MoltResult&)>;

    virtual ~IScheduler() = default;

    /**
     * @brief Schedule task for execution
     * @param task Task to execute
     * @param callback Callback invoked on I/O thread when task completes
     * @return true if task was scheduled successfully
     */
    virtual auto schedule(const MoltTask& task, Callback callback) -> bool = 0;

    /**
     * @brief Schedule delayed task
     * @param task Task to execute
     * @param delay_ms Delay before execution (milliseconds)
     * @param callback Callback invoked on I/O thread when task completes
     * @return true if task was scheduled successfully
     */
    virtual auto schedule_delayed(
        const MoltTask& task,
        uint64_t delay_ms,
        Callback callback
    ) -> bool = 0;

    /**
     * @brief Schedule task with timeout
     * @param task Task to execute
     * @param timeout_ms Timeout for task execution
     * @param callback Callback invoked on I/O thread when task completes or times out
     * @return true if task was scheduled successfully
     */
    virtual auto schedule_with_timeout(
        const MoltTask& task,
        uint64_t timeout_ms,
        Callback callback
    ) -> bool = 0;

    /**
     * @brief Cancel pending task
     * @param task_id ID of task to cancel
     * @return true if task was found and cancelled
     */
    virtual auto cancel(const char* task_id) -> bool = 0;

    /**
     * @brief Get task status
     * @param task_id ID of task to query
     * @return Task status, or nullptr if not found
     */
    [[nodiscard]] virtual auto get_status(const char* task_id) const -> TaskStatus = 0;

    /**
     * @brief Scheduler statistics
     */
    struct Stats {
        size_t pending = 0;      // Tasks waiting to execute
        size_t running = 0;      // Tasks currently executing
        size_t completed = 0;    // Successfully completed tasks
        size_t failed = 0;       // Failed tasks
        size_t timed_out = 0;    // Tasks that timed out
    };

    [[nodiscard]] virtual auto get_stats() const -> Stats = 0;

    /**
     * @brief Shutdown scheduler
     * @param wait_for_completion If true, wait for running tasks to complete
     */
    virtual auto shutdown(bool wait_for_completion = true) -> void = 0;

    /**
     * @brief Check if scheduler is running
     */
    [[nodiscard]] virtual auto is_running() const -> bool = 0;
};

// ========================================================================
// Template Scheduler Implementation
// ========================================================================

/**
 * @brief Task scheduler template
 *
 * Works with any executor type that satisfies the ExecutorLike concept:
 * - Executor
 * - HybridExecutor
 * - Custom executor implementations
 *
 * Architecture:
 * ```
 * I/O Thread              Scheduler              Executor
 *     |                       |                       |
 *     |  schedule(task)       |                       |
 *     |--------------------->|                       |
 *     |                       |                       |
 *     |                       |  submit(work)         |
 *     |                       |---------------------->|
 *     |                       |                       |
 *     |                       |              [execute skill]
 *     |                       |                       |
 *     |                       |  return result        |
 *     |                       |<----------------------|
 *     |                       |                       |
 *     |  callback(result)     |                       |
 *     |<---------------------|                       |
 * ```
 *
 * Usage with Executor:
 * @code
 * EventLoop io_loop;
 * Executor executor(4);
 * Scheduler<Executor> scheduler(io_loop, executor);
 *
 * scheduler.schedule(task, [](const MoltResult& result) {
 *     // Handle result on I/O thread
 * });
 * @endcode
 *
 * Usage with HybridExecutor:
 * @code
 * EventLoop io_loop;
 * HybridExecutor executor(io_loop, 4);
 * Scheduler<HybridExecutor> scheduler(io_loop, executor);
 *
 * // Same interface, tasks routed based on TaskType
 * scheduler.schedule(task, [](const MoltResult& result) {
 *     // Handle result
 * });
 * @endcode
 */
template <typename ExecutorType>
class Scheduler : public IScheduler {
public:
    using Callback = std::function<void(const MoltResult&)>;

    /**
     * @brief Create scheduler
     * @param io_loop Event loop for I/O operations (callbacks run here)
     * @param executor Thread pool for CPU work
     * @param max_concurrent Maximum concurrent tasks (0 = unlimited)
     */
    Scheduler(
        EventLoop& io_loop,
        ExecutorType& executor,
        size_t max_concurrent = 0
    );

    ~Scheduler() override;

    // Prevent copy
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    // Allow move
    Scheduler(Scheduler&&) noexcept;
    Scheduler& operator=(Scheduler&&) noexcept;

    // ========================================================================
    // IScheduler implementation
    // ========================================================================

    /**
     * @brief Schedule task for execution
     * @param task Task to execute
     * @param callback Callback invoked on I/O thread when task completes
     * @return true if task was scheduled successfully
     *
     * The callback is guaranteed to be called on the I/O event loop thread,
     * making it safe to perform I/O operations within the callback.
     */
    auto schedule(const MoltTask& task, Callback callback) -> bool override;

    /**
     * @brief Schedule delayed task
     * @param task Task to execute
     * @param delay_ms Delay before execution (milliseconds)
     * @param callback Callback invoked on I/O thread when task completes
     * @return true if task was scheduled successfully
     */
    auto schedule_delayed(
        const MoltTask& task,
        uint64_t delay_ms,
        Callback callback
    ) -> bool override;

    /**
     * @brief Schedule task with timeout
     * @param task Task to execute
     * @param timeout_ms Timeout for task execution
     * @param callback Callback invoked on I/O thread when task completes or times out
     * @return true if task was scheduled successfully
     *
     * If task execution exceeds timeout_ms, callback will be invoked
     * with a timeout error result.
     */
    auto schedule_with_timeout(
        const MoltTask& task,
        uint64_t timeout_ms,
        Callback callback
    ) -> bool override;

    /**
     * @brief Cancel pending task
     * @param task_id ID of task to cancel
     * @return true if task was found and cancelled
     *
     * Note: Already running tasks cannot be cancelled.
     */
    auto cancel(const char* task_id) -> bool override;

    /**
     * @brief Get task status
     * @param task_id ID of task to query
     * @return Task status, or nullptr if not found
     */
    [[nodiscard]] auto get_status(const char* task_id) const -> TaskStatus override;

    /**
     * @brief Get statistics
     */
    [[nodiscard]] auto get_stats() const -> Stats override;

    /**
     * @brief Shutdown scheduler
     * @param wait_for_completion If true, wait for running tasks to complete
     */
    auto shutdown(bool wait_for_completion = true) -> void override;

    /**
     * @brief Check if scheduler is running
     */
    [[nodiscard]] auto is_running() const -> bool override;

    // ========================================================================
    // Executor-specific access
    // ========================================================================

    /**
     * @brief Get underlying executor
     */
    [[nodiscard]] auto get_executor() const noexcept -> ExecutorType& {
        return executor_;
    }

    /**
     * @brief Get underlying executor (non-const)
     */
    [[nodiscard]] auto get_executor() noexcept -> ExecutorType& {
        return executor_;
    }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    // Task entry tracking
    struct TaskEntry {
        ScheduledTask task;
        std::shared_ptr<std::atomic<bool>> active;
    };

    mutable std::mutex tasks_mutex_;
    std::unordered_map<std::string, TaskEntry> pending_tasks_;
    std::unordered_map<std::string, TaskEntry> running_tasks_;

    EventLoop& io_loop_;
    ExecutorType& executor_;

    std::atomic<bool> running_{true};
    std::atomic<size_t> max_concurrent_{0};
    std::atomic<size_t> currently_running_{0};

    // Statistics
    std::atomic<size_t> stats_completed_{0};
    std::atomic<size_t> stats_failed_{0};
    std::atomic<size_t> stats_timed_out_{0};

    // Internal methods
    auto execute_task(ScheduledTask& task) -> void;
    auto handle_completion(const std::string& task_id, MoltResult result) -> void;
    auto check_timeouts() -> void;
    auto post_callback(Callback callback, MoltResult result) -> void;
};

// ========================================================================
// Type-Erased Scheduler (Runtime Polymorphism)
// ========================================================================

/**
 * @brief Type-erased scheduler wrapper
 *
 * Provides runtime polymorphism for scheduler implementations.
 * Use this when you need to store or pass schedutors heterogeneously.
 *
 * Usage:
 * @code
 * // Create with Executor
 * EventLoop io_loop;
 * Executor exec(4);
 * Scheduler<Executor> sched(io_loop, exec);
 * AnyScheduler any_sched(std::move(sched));
 *
 * // Or create with HybridExecutor
 * HybridExecutor hybrid(io_loop, 4);
 * Scheduler<HybridExecutor> sched2(io_loop, hybrid);
 * AnyScheduler any_sched2(std::move(sched2));
 *
 * // Use through interface
 * void process_tasks(AnyScheduler& scheduler) {
 *     scheduler.schedule(task, callback);
 * }
 * @endcode
 */
class AnyScheduler {
public:
    using Callback = IScheduler::Callback;
    using Stats = IScheduler::Stats;

    // ========================================================================
    // Constructors
    // ========================================================================

    /**
     * @brief Default constructor (creates null scheduler)
     */
    AnyScheduler() = default;

    /**
     * @brief Construct from any IScheduler implementation
     * @tparam SchedulerType Scheduler type (must inherit from IScheduler)
     */
    template <typename SchedulerType>
    // NOLINTNEXTLINE(google-explicit-constructor)
    AnyScheduler(SchedulerType&& scheduler)
        requires std::derived_from<std::remove_reference_t<SchedulerType>, IScheduler>
        : holder_(std::make_unique<Model<std::remove_reference_t<SchedulerType>>>(
            std::forward<SchedulerType>(scheduler))) {}

    /**
     * @brief Construct from Executor (creates Scheduler internally)
     */
    AnyScheduler(EventLoop& io_loop, Executor& executor, size_t max_concurrent = 0);

    /**
     * @brief Construct from HybridExecutor (creates Scheduler internally)
     */
    AnyScheduler(EventLoop& io_loop, HybridExecutor& executor, size_t max_concurrent = 0);

    // Move support
    AnyScheduler(AnyScheduler&&) noexcept = default;
    AnyScheduler& operator=(AnyScheduler&&) noexcept = default;

    // Disable copy
    AnyScheduler(const AnyScheduler&) = delete;
    AnyScheduler& operator=(const AnyScheduler&) = delete;

    // ========================================================================
    // IScheduler interface forwarding
    // ========================================================================

    /**
     * @brief Check if scheduler is valid (not null)
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return holder_ != nullptr;
    }

    /**
     * @brief Explicit bool conversion
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return is_valid();
    }

    /**
     * @brief Schedule task for execution
     */
    auto schedule(const MoltTask& task, Callback callback) -> bool {
        return holder_->schedule(task, std::move(callback));
    }

    /**
     * @brief Schedule delayed task
     */
    auto schedule_delayed(
        const MoltTask& task,
        uint64_t delay_ms,
        Callback callback
    ) -> bool {
        return holder_->schedule_delayed(task, delay_ms, std::move(callback));
    }

    /**
     * @brief Schedule task with timeout
     */
    auto schedule_with_timeout(
        const MoltTask& task,
        uint64_t timeout_ms,
        Callback callback
    ) -> bool {
        return holder_->schedule_with_timeout(task, timeout_ms, std::move(callback));
    }

    /**
     * @brief Cancel pending task
     */
    auto cancel(const char* task_id) -> bool {
        return holder_->cancel(task_id);
    }

    /**
     * @brief Get task status
     */
    [[nodiscard]] auto get_status(const char* task_id) const -> TaskStatus {
        return holder_->get_status(task_id);
    }

    /**
     * @brief Get statistics
     */
    [[nodiscard]] auto get_stats() const -> Stats {
        return holder_->get_stats();
    }

    /**
     * @brief Shutdown scheduler
     */
    auto shutdown(bool wait_for_completion = true) -> void {
        if (holder_) holder_->shutdown(wait_for_completion);
    }

    /**
     * @brief Check if scheduler is running
     */
    [[nodiscard]] auto is_running() const -> bool {
        return holder_ && holder_->is_running();
    }

    /**
     * @brief Get underlying IScheduler pointer
     */
    [[nodiscard]] auto get() const noexcept -> IScheduler* {
        return holder_.get();
    }

    /**
     * @brief Get underlying IScheduler pointer (non-const)
     */
    [[nodiscard]] auto get() noexcept -> IScheduler* {
        return holder_.get();
    }

private:
    // Type erasure using small buffer optimization
    class Holder {
    public:
        virtual ~Holder() = default;

        virtual auto schedule(const MoltTask&, Callback&&) -> bool = 0;
        virtual auto schedule_delayed(const MoltTask&, uint64_t, Callback&&) -> bool = 0;
        virtual auto schedule_with_timeout(const MoltTask&, uint64_t, Callback&&) -> bool = 0;
        virtual auto cancel(const char*) -> bool = 0;
        [[nodiscard]] virtual auto get_status(const char*) const -> TaskStatus = 0;
        [[nodiscard]] virtual auto get_stats() const -> Stats = 0;
        virtual auto shutdown(bool) -> void = 0;
        [[nodiscard]] virtual auto is_running() const -> bool = 0;
    };

    template <typename SchedulerType>
    class Model : public Holder {
    public:
        explicit Model(SchedulerType&& sched) : scheduler_(std::move(sched)) {}

        auto schedule(const MoltTask& task, Callback&& callback) -> bool override {
            return scheduler_.schedule(task, std::move(callback));
        }

        auto schedule_delayed(const MoltTask& task, uint64_t delay_ms, Callback&& callback) -> bool override {
            return scheduler_.schedule_delayed(task, delay_ms, std::move(callback));
        }

        auto schedule_with_timeout(const MoltTask& task, uint64_t timeout_ms, Callback&& callback) -> bool override {
            return scheduler_.schedule_with_timeout(task, timeout_ms, std::move(callback));
        }

        auto cancel(const char* task_id) -> bool override {
            return scheduler_.cancel(task_id);
        }

        [[nodiscard]] auto get_status(const char* task_id) const -> TaskStatus override {
            return scheduler_.get_status(task_id);
        }

        [[nodiscard]] auto get_stats() const -> Stats override {
            return scheduler_.get_stats();
        }

        auto shutdown(bool wait_for_completion) -> void override {
            scheduler_.shutdown(wait_for_completion);
        }

        [[nodiscard]] auto is_running() const -> bool override {
            return scheduler_.is_running();
        }

    private:
        SchedulerType scheduler_;
    };

    std::unique_ptr<Holder> holder_;
};

// ========================================================================
// Convenience Type Aliases
// ========================================================================

/**
 * @brief Scheduler using Executor (CPU thread pool only)
 */
using CPUScheduler = Scheduler<Executor>;

/**
 * @brief Scheduler using HybridExecutor (intelligent routing)
 */
using HybridScheduler = Scheduler<HybridExecutor>;

// ========================================================================
// Inline Helper Functions
// ========================================================================

/**
 * @brief Schedule task and return future
 *
 * Works with any scheduler type.
 *
 * Usage:
 * @code
 * auto future = schedule_future(scheduler, task);
 * auto result = future.get();  // Blocks until completion
 * @endcode
 */
template <typename SchedulerType>
[[nodiscard]] inline auto schedule_future(
    SchedulerType& scheduler,
    const MoltTask& task
) -> std::future<MoltResult> {
    auto promise = std::make_shared<std::promise<MoltResult>>();
    auto future = promise->get_future();

    scheduler.schedule(task, [p = std::move(promise)](const MoltResult& result) {
        p->set_value(result);
    });

    return future;
}

/**
 * @brief Schedule task with timeout and return future
 *
 * If timeout occurs, future will contain an error result.
 */
template <typename SchedulerType>
[[nodiscard]] inline auto schedule_future_with_timeout(
    SchedulerType& scheduler,
    const MoltTask& task,
    uint64_t timeout_ms
) -> std::future<MoltResult> {
    auto promise = std::make_shared<std::promise<MoltResult>>();
    auto future = promise->get_future();

    scheduler.schedule_with_timeout(task, timeout_ms,
        [p = std::move(promise)](const MoltResult& result) {
            p->set_value(result);
        });

    return future;
}

/**
 * @brief Schedule task and return future (AnyScheduler overload)
 */
[[nodiscard]] inline auto schedule_future(
    AnyScheduler& scheduler,
    const MoltTask& task
) -> std::future<MoltResult> {
    auto promise = std::make_shared<std::promise<MoltResult>>();
    auto future = promise->get_future();

    scheduler.schedule(task, [p = std::move(promise)](const MoltResult& result) {
        p->set_value(result);
    });

    return future;
}

/**
 * @brief Schedule task with timeout and return future (AnyScheduler overload)
 */
[[nodiscard]] inline auto schedule_future_with_timeout(
    AnyScheduler& scheduler,
    const MoltTask& task,
    uint64_t timeout_ms
) -> std::future<MoltResult> {
    auto promise = std::make_shared<std::promise<MoltResult>>();
    auto future = promise->get_future();

    scheduler.schedule_with_timeout(task, timeout_ms,
        [p = std::move(promise)](const MoltResult& result) {
            p->set_value(result);
        });

    return future;
}

// ========================================================================
// Coroutine Integration (C++20)
// ========================================================================

namespace detail {

/**
 * @brief Awaitable for scheduling task on scheduler
 */
template <typename SchedulerType>
struct ScheduleAwaitable {
    SchedulerType& scheduler;
    const MoltTask& task;
    std::shared_ptr<std::promise<MoltResult>> promise;

    explicit ScheduleAwaitable(SchedulerType& s, const MoltTask& t)
        : scheduler(s)
        , task(t)
        , promise(std::make_shared<std::promise<MoltResult>>()) {}

    auto await_ready() const noexcept -> bool { return false; }

    auto await_suspend(std::coroutine_handle<> handle) -> void {
        scheduler.schedule(task, [this, handle](const MoltResult& result) {
            promise->set_value(result);
            handle.resume();
        });
    }

    [[nodiscard]] auto await_resume() -> MoltResult {
        return promise->get_future().get();
    }
};

/**
 * @brief Awaitable for AnyScheduler
 */
template <>
struct ScheduleAwaitable<AnyScheduler> {
    AnyScheduler& scheduler;
    const MoltTask& task;
    std::shared_ptr<std::promise<MoltResult>> promise;

    explicit ScheduleAwaitable(AnyScheduler& s, const MoltTask& t)
        : scheduler(s)
        , task(t)
        , promise(std::make_shared<std::promise<MoltResult>>()) {}

    auto await_ready() const noexcept -> bool { return false; }

    auto await_suspend(std::coroutine_handle<> handle) -> void {
        scheduler.schedule(task, [this, handle](const MoltResult& result) {
            promise->set_value(result);
            handle.resume();
        });
    }

    [[nodiscard]] auto await_resume() -> MoltResult {
        return promise->get_future().get();
    }
};

} // namespace detail

/**
 * @brief Schedule task as coroutine awaitable
 *
 * Usage in coroutine:
 * @code
 * auto process_task(Scheduler<Executor>& scheduler, MoltTask task) -> Task<void> {
 *     auto result = co_await schedule_async(scheduler, task);
 *     if (result.success) {
 *         handle_success(result);
 *     }
 * }
 * @endcode
 */
template <typename SchedulerType>
[[nodiscard]] inline auto schedule_async(SchedulerType& scheduler, const MoltTask& task)
    -> detail::ScheduleAwaitable<SchedulerType> {
    return {scheduler, task};
}

} // namespace moltcat::core
