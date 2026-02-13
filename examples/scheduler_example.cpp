/**
 * @file scheduler_example.cpp
 * @brief Example demonstrating unified scheduler interface
 *
 * This example shows:
 * 1. Template Scheduler with Executor
 * 2. Template Scheduler with HybridExecutor
 * 3. IScheduler interface for polymorphism
 * 4. AnyScheduler for runtime polymorphism
 */

#include <moltcat/core/scheduler.hpp>
#include <moltcat/core/event_loop.hpp>
#include <moltcat/core/executor.hpp>
#include <moltcat/core/hybrid_executor.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using namespace moltcat::core;

// ========================================================================
// Example 1: Template Scheduler with Executor
// ========================================================================

void example_cpu_scheduler() {
    std::cout << "\n=== Example 1: CPUScheduler (Executor) ===\n" << std::endl;

    EventLoop io_loop;
    Executor executor(4);
    CPUScheduler scheduler(io_loop, executor);

    std::atomic<int> completed{0};

    // Create and schedule tasks
    for (int i = 0; i < 5; ++i) {
        MoltTask task;
        task.id = IString::create("task-" + std::to_string(i));
        task.type = IString::create("compute");
        task.payload = IString::create("{\"n\": " + std::to_string(i * 10) + "}");
        task.priority = TaskPriority::NORMAL;

        scheduler.schedule(task, [&completed, i](const MoltResult& result) {
            std::cout << "Task " << i << " completed on I/O thread, "
                      << "success: " << result.success << std::endl;
            completed++;
        });
    }

    // Run I/O loop briefly to process callbacks
    std::thread io_thread([&] {
        io_loop.run();
    });

    // Wait for completion
    while (completed.load() < 5) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    io_loop.stop();
    io_thread.join();

    auto stats = scheduler.get_stats();
    std::cout << "Stats: completed=" << stats.completed
              << ", failed=" << stats.failed << std::endl;

    // Cleanup
    for (int i = 0; i < 5; ++i) {
        // Note: In real code, task IDs would need proper cleanup
    }
}

// ========================================================================
// Example 2: Template Scheduler with HybridExecutor
// ========================================================================

void example_hybrid_scheduler() {
    std::cout << "\n=== Example 2: HybridScheduler (HybridExecutor) ===\n" << std::endl;

    EventLoop io_loop;
    HybridExecutor executor(io_loop, 4);
    HybridScheduler scheduler(io_loop, executor);

    std::atomic<int> completed{0};

    // Schedule I/O tasks (routes to libuv pool)
    for (int i = 0; i < 3; ++i) {
        MoltTask task;
        task.id = IString::create("io-task-" + std::to_string(i));
        task.type = IString::create("file_read");
        task.payload = IString::create("{\"path\": \"data" + std::to_string(i) + ".txt\"}");
        task.priority = TaskPriority::NORMAL;

        scheduler.schedule(task, [&completed, i](const MoltResult& result) {
            std::cout << "I/O Task " << i << " completed, "
                      << "success: " << result.success << std::endl;
            completed++;
        });
    }

    // Schedule CPU tasks (routes to CPU pool)
    for (int i = 0; i < 3; ++i) {
        MoltTask task;
        task.id = IString::create("cpu-task-" + std::to_string(i));
        task.type = IString::create("compute");
        task.payload = IString::create("{\"n\": " + std::to_string(i * 20) + "}");
        task.priority = TaskPriority::NORMAL;

        scheduler.schedule(task, [&completed, i](const MoltResult& result) {
            std::cout << "CPU Task " << i << " completed, "
                      << "success: " << result.success << std::endl;
            completed++;
        });
    }

    // Run I/O loop
    std::thread io_thread([&] {
        io_loop.run();
    });

    while (completed.load() < 6) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    io_loop.stop();
    io_thread.join();

    auto stats = scheduler.get_stats();
    std::cout << "Stats: completed=" << stats.completed
              << ", failed=" << stats.failed << std::endl;
}

// ========================================================================
// Example 3: IScheduler Interface (Compile-time Polymorphism)
// ========================================================================

/**
 * @brief Service that works with any scheduler type
 *
 * Using IScheduler reference for compile-time polymorphism.
 */
class TaskProcessor {
public:
    explicit TaskProcessor(IScheduler& scheduler) : scheduler_(scheduler) {}

    auto process_task(const std::string& task_id, const std::string& payload) -> void {
        MoltTask task;
        task.id = IString::create(task_id);
        task.type = IString::create("process");
        task.payload = IString::create(payload);
        task.priority = TaskPriority::NORMAL;

        scheduler_.schedule(task, [task_id](const MoltResult& result) {
            std::cout << "Task " << task_id << " "
                      << (result.success ? "succeeded" : "failed") << std::endl;
        });
    }

    auto process_with_timeout(const std::string& task_id, uint64_t timeout_ms) -> void {
        MoltTask task;
        task.id = IString::create(task_id);
        task.type = IString::create("process");
        task.payload = IString::create("{}");
        task.priority = TaskPriority::NORMAL;

        scheduler_.schedule_with_timeout(task, timeout_ms, [task_id](const MoltResult& result) {
            std::cout << "Task " << task_id << " "
                      << (result.success ? "succeeded" : "failed/timed out") << std::endl;
        });
    }

private:
    IScheduler& scheduler_;
};

void example_ischeduler_interface() {
    std::cout << "\n=== Example 3: IScheduler Interface ===\n" << std::endl;

    EventLoop io_loop;

    // Can use Executor or HybridExecutor
    Executor cpu_exec(2);
    CPUScheduler cpu_sched(io_loop, cpu_exec);

    std::cout << "\n--- Using CPUScheduler ---" << std::endl;
    TaskProcessor processor1(cpu_sched);
    processor1.process_task("task-1", "{\"data\": 1}");
    processor1.process_task("task-2", "{\"data\": 2}");

    std::thread io_thread1([&] { io_loop.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    io_loop.stop();
    io_thread1.join();

    // Or use HybridExecutor
    io_loop = EventLoop();  // Reset
    HybridExecutor hybrid_exec(io_loop, 2);
    HybridScheduler hybrid_sched(io_loop, hybrid_exec);

    std::cout << "\n--- Using HybridScheduler ---" << std::endl;
    TaskProcessor processor2(hybrid_sched);
    processor2.process_task("task-3", "{\"data\": 3}");
    processor2.process_task("task-4", "{\"data\": 4}");

    std::thread io_thread2([&] { io_loop.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    io_loop.stop();
    io_thread2.join();
}

// ========================================================================
// Example 4: AnyScheduler (Runtime Polymorphism)
// ========================================================================

void example_anyscheduler() {
    std::cout << "\n=== Example 4: AnyScheduler (Runtime Polymorphism) ===\n" << std::endl;

    // Store different schedulers in container
    std::vector<AnyScheduler> schedulers;

    EventLoop io_loop1;
    Executor exec1(2);
    schedulers.emplace_back(io_loop1, exec1);

    EventLoop io_loop2;
    HybridExecutor exec2(io_loop2, 2);
    schedulers.emplace_back(io_loop2, exec2);

    std::atomic<int> completed{0};

    // Process tasks using AnyScheduler
    for (size_t i = 0; i < schedulers.size(); ++i) {
        MoltTask task;
        task.id = IString::create("any-task-" + std::to_string(i));
        task.type = IString::create("process");
        task.payload = IString::create("{}");
        task.priority = TaskPriority::NORMAL;

        schedulers[i].schedule(task, [&completed, i](const MoltResult& result) {
            std::cout << "AnyScheduler " << i << " task completed, "
                      << "success: " << result.success << std::endl;
            completed++;
        });
    }

    // Run I/O loops
    std::thread io_thread1([&] { io_loop1.run(); });
    std::thread io_thread2([&] { io_loop2.run(); });

    while (completed.load() < 2) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    io_loop1.stop();
    io_loop2.stop();
    io_thread1.join();
    io_thread2.join();
}

// ========================================================================
// Example 5: Scheduler Factory
// ========================================================================

enum class SchedulerType {
    CPU_ONLY,
    HYBRID
};

/**
 * @brief Factory function to create scheduler based on configuration
 */
auto create_scheduler(SchedulerType type, EventLoop& io_loop, size_t num_threads) -> AnyScheduler {
    switch (type) {
        case SchedulerType::CPU_ONLY: {
            static Executor executor(num_threads);
            return AnyScheduler(io_loop, executor);
        }
        case SchedulerType::HYBRID: {
            static HybridExecutor executor(io_loop, num_threads);
            return AnyScheduler(io_loop, executor);
        }
    }
    // Unreachable
    return AnyScheduler();
}

void example_scheduler_factory() {
    std::cout << "\n=== Example 5: Scheduler Factory ===\n" << std::endl;

    EventLoop io_loop;

    // Create based on runtime configuration
    auto use_hybrid = true;
    auto scheduler = create_scheduler(
        use_hybrid ? SchedulerType::HYBRID : SchedulerType::CPU_ONLY,
        io_loop,
        4
    );

    if (!scheduler.is_valid()) {
        std::cout << "Failed to create scheduler" << std::endl;
        return;
    }

    std::atomic<bool> done{false};

    MoltTask task;
    task.id = IString::create("factory-task");
    task.type = IString::create("process");
    task.payload = IString::create("{}");
    task.priority = TaskPriority::NORMAL;

    scheduler.schedule(task, [&done](const MoltResult& result) {
        std::cout << "Factory task completed, success: " << result.success << std::endl;
        done = true;
    });

    std::thread io_thread([&] { io_loop.run(); });
    while (!done.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    io_loop.stop();
    io_thread.join();
}

// ========================================================================
// Example 6: Future-based API
// ========================================================================

void example_future_api() {
    std::cout << "\n=== Example 6: Future-based API ===\n" << std::endl;

    EventLoop io_loop;
    Executor executor(2);
    CPUScheduler scheduler(io_loop, executor);

    std::thread io_thread([&] { io_loop.run(); });

    // Schedule task and get future
    MoltTask task;
    task.id = IString::create("future-task");
    task.type = IString::create("compute");
    task.payload = IString::create("{}");
    task.priority = TaskPriority::NORMAL;

    auto future = schedule_future(scheduler, task);
    // future.get() would block, so we don't call it here
    // In real code, you might use future.wait_for() or wait_until()

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    io_loop.stop();
    io_thread.join();

    std::cout << "Future-based API demo completed" << std::endl;
}

// ========================================================================
// Example 7: Template Function with Concept
// ========================================================================

template <typename SchedulerType>
concept SchedulerLike = requires(SchedulerType& s, const MoltTask& t, IScheduler::Callback&& cb) {
    { s.schedule(t, std::move(cb)) } -> std::same_as<bool>;
    { s.get_stats() } -> std::convertible_to<IScheduler::Stats>;
};

/**
 * @brief Generic task processor using concept constraint
 */
template <SchedulerLike S>
auto process_multiple_tasks(S& scheduler, int count) -> void {
    std::atomic<int> completed{0};

    for (int i = 0; i < count; ++i) {
        MoltTask task;
        task.id = IString::create("concept-task-" + std::to_string(i));
        task.type = IString::create("process");
        task.payload = IString::create("{}");
        task.priority = TaskPriority::NORMAL;

        scheduler.schedule(task, [&completed, i](const MoltResult& result) {
            std::cout << "Concept task " << i << " done" << std::endl;
            completed++;
        });
    }

    // In real code, wait for completion
}

void example_concept_based() {
    std::cout << "\n=== Example 7: Concept-based Generic Function ===\n" << std::endl;

    EventLoop io_loop;

    // Works with CPUScheduler
    Executor cpu_exec(2);
    CPUScheduler cpu_sched(io_loop, cpu_exec);
    process_multiple_tasks(cpu_sched, 3);

    std::thread io_thread([&] { io_loop.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    io_loop.stop();
    io_thread.join();
}

// ========================================================================
// Main
// ========================================================================

int main() {
    std::cout << "MoltCat Unified Scheduler Examples\n";
    std::cout << "Hardware concurrency: "
              << std::thread::hardware_concurrency() << " threads\n" << std::endl;

    example_cpu_scheduler();
    example_hybrid_scheduler();
    example_ischeduler_interface();
    example_anyscheduler();
    example_scheduler_factory();
    example_future_api();
    example_concept_based();

    std::cout << "\n=== All Examples Complete ===" << std::endl;

    return 0;
}
