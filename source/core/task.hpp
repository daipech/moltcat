/**
 * @file task.hpp
 * @brief Unified task definition for all executors
 *
 * This file defines the common Task type used by both Executor and HybridExecutor.
 * Upper layer code can switch executor implementations without changing task code.
 */

#pragma once

#include <functional>
#include <cstdint>

namespace moltcat::core {

/**
 * @brief Unified task definition for executors
 *
 * A Task encapsulates:
 * - The function to execute
 * - Task type hint (for routing decisions in HybridExecutor)
 * - Priority level (for scheduling in Executor)
 * - Optional metadata (enqueue time, affinity)
 *
 * Usage:
 * @code
 * // Simple task with defaults
 * executor.submit(Task{[] { do_work(); }});
 *
 * // Task with type hint
 * executor.submit(Task{[] { do_io(); }, TaskType::IO_BOUND});
 *
 * // Task with priority
 * executor.submit(Task{[] { urgent_work(); }, Task::Priority::HIGH});
 *
 * // Task with both type and priority
 * executor.submit(Task{
 *     [] { work(); },
 *     TaskType::CPU_HEAVY,
 *     Task::Priority::HIGH
 * });
 * @endcode
 */
class Task {
public:
    /**
     * @brief Task priority levels
     *
     * Higher priority tasks are executed before lower priority tasks.
     * Tasks with same priority are executed in FIFO order.
     */
    enum class Priority : int {
        LOW = 0,         ///< Low priority, executed when no other tasks
        NORMAL = 1,      ///< Default priority for normal tasks
        HIGH = 2,        ///< High priority, executed before normal tasks
        CRITICAL = 3     ///< Critical priority, executed as soon as possible
    };

    /**
     * @brief Task type classification
     *
     * Used by HybridExecutor to route tasks to appropriate thread pool:
     * - IO_BOUND, CPU_LIGHT → libuv thread pool
     * - CPU_HEAVY, PRIORITIZED → custom CPU thread pool
     *
     * Executor ignores this field and routes all tasks to its worker threads.
     */
    enum class TaskType : int {
        IO_BOUND,       ///< File I/O, DNS queries (routes to libuv in HybridExecutor)
        CPU_LIGHT,      ///< Quick computation < 1ms (routes to libuv in HybridExecutor)
        CPU_HEAVY,      ///< Heavy computation > 10ms (routes to CPU pool in HybridExecutor)
        PRIORITIZED     ///< Requires priority scheduling (routes to CPU pool in HybridExecutor)
    };

    /**
     * @brief Task function type
     *
     * The function to execute. Should be noexcept if possible for better performance.
     */
    using Func = std::function<void()>;

    // ========================================================================
    // Constructors
    // ========================================================================

    /**
     * @brief Default constructor (creates empty task)
     */
    Task() = default;

    /**
     * @brief Construct task with function only
     * @param f Function to execute
     *
     * Uses default TaskType::CPU_HEAVY and Priority::NORMAL
     */
    explicit Task(Func&& f) noexcept
        : func(std::move(f))
        , type(TaskType::CPU_HEAVY)
        , priority(Priority::NORMAL) {}

    /**
     * @brief Construct task with function and type
     * @param f Function to execute
     * @param t Task type hint
     *
     * Uses default Priority::NORMAL
     */
    Task(Func&& f, TaskType t) noexcept
        : func(std::move(f))
        , type(t)
        , priority(Priority::NORMAL) {}

    /**
     * @brief Construct task with function and priority
     * @param f Function to execute
     * @param p Task priority
     *
     * Uses default TaskType::PRIORITIZED (since priority is specified)
     */
    Task(Func&& f, Priority p) noexcept
        : func(std::move(f))
        , type(TaskType::PRIORITIZED)
        , priority(p) {}

    /**
     * @brief Construct task with function, type, and priority
     * @param f Function to execute
     * @param t Task type hint
     * @param p Task priority
     */
    Task(Func&& f, TaskType t, Priority p) noexcept
        : func(std::move(f))
        , type(t)
        , priority(p) {}

    /**
     * @brief Execute the task
     */
    auto operator()() -> void { func(); }

    /**
     * @brief Check if task is valid (has a function)
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return func != nullptr;
    }

    // ========================================================================
    // Task members
    // ========================================================================

    Func func;                     ///< Function to execute
    TaskType type = TaskType::CPU_HEAVY;  ///< Task type hint for routing
    Priority priority = Priority::NORMAL; ///< Task priority for scheduling

    // Optional metadata (used by Executor)
    uint64_t enqueue_time = 0;     ///< Time when task was enqueued (for FIFO aging)
    int affinity = -1;              ///< Thread affinity (-1 = any thread)
};

/**
 * @brief Comparison functor for priority queue
 *
 * Compares tasks first by priority, then by enqueue time.
 * Higher priority tasks come first; same priority uses FIFO (older first).
 */
struct TaskComparator {
    [[nodiscard]] auto operator()(const Task& a, const Task& b) const noexcept -> bool {
        // Higher priority = should be executed first (lower value in priority queue)
        if (a.priority != b.priority) {
            return static_cast<int>(a.priority) < static_cast<int>(b.priority);
        }
        // Same priority: older tasks first (FIFO)
        return a.enqueue_time > b.enqueue_time;
    }
};

// ========================================================================
// Convenience Builders
// ========================================================================

/**
 * @brief Create a task with specific type
 *
 * Usage:
 * @code
 * auto task = with_type([] { do_io(); }, TaskType::IO_BOUND);
 * executor.submit(std::move(task));
 * @endcode
 */
[[nodiscard]] inline auto with_type(Task::Func&& f, Task::TaskType t) noexcept -> Task {
    return Task(std::move(f), t);
}

/**
 * @brief Create a task with specific priority
 *
 * Usage:
 * @code
 * auto task = with_priority([] { urgent(); }, Task::Priority::HIGH);
 * executor.submit(std::move(task));
 * @endcode
 */
[[nodiscard]] inline auto with_priority(Task::Func&& f, Task::Priority p) noexcept -> Task {
    return Task(std::move(f), p);
}

/**
 * @brief Create a task with both type and priority
 *
 * Usage:
 * @code
 * auto task = task_with([] { work(); }, TaskType::CPU_HEAVY, Task::Priority::HIGH);
 * executor.submit(std::move(task));
 * @endcode
 */
[[nodiscard]] inline auto task_with(
    Task::Func&& f,
    Task::TaskType t,
    Task::Priority p
) noexcept -> Task {
    return Task(std::move(f), t, p);
}

// ========================================================================
// Pre-defined type shortcuts
// ========================================================================

/**
 * @brief Create an I/O-bound task
 *
 * Routes to libuv thread pool in HybridExecutor.
 */
[[nodiscard]] inline auto io_task(Task::Func&& f) noexcept -> Task {
    return Task(std::move(f), Task::TaskType::IO_BOUND);
}

/**
 * @brief Create a light CPU task
 *
 * Routes to libuv thread pool in HybridExecutor for quick computations.
 */
[[nodiscard]] inline auto light_cpu_task(Task::Func&& f) noexcept -> Task {
    return Task(std::move(f), Task::TaskType::CPU_LIGHT);
}

/**
 * @brief Create a heavy CPU task
 *
 * Routes to CPU thread pool in HybridExecutor for intensive computations.
 */
[[nodiscard]] inline auto cpu_task(Task::Func&& f) noexcept -> Task {
    return Task(std::move(f), Task::TaskType::CPU_HEAVY);
}

/**
 * @brief Create a prioritized task
 *
 * Routes to CPU thread pool in HybridExecutor with priority scheduling.
 */
[[nodiscard]] inline auto prioritized_task(Task::Func&& f, Task::Priority p) noexcept -> Task {
    return Task(std::move(f), Task::TaskType::PRIORITIZED, p);
}

} // namespace moltcat::core
