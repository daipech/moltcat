/**
 * @file unified_executor_example.cpp
 * @brief Example demonstrating unified executor interface
 *
 * This example shows how Executor and HybridExecutor share the same interface,
 * allowing upper layer code to switch implementations without changes.
 */

#include <moltcat/core/task.hpp>
#include <moltcat/core/executor.hpp>
#include <moltcat/core/hybrid_executor.hpp>
#include <moltcat/core/event_loop.hpp>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

using namespace moltcat::core;

// ========================================================================
// Upper Layer Service (executor-agnostic)
// ========================================================================

/**
 * @brief Example service that works with any executor type
 *
 * This demonstrates how upper layer code can be written to work
 * with both Executor and HybridExecutor without changes.
 */
template <typename ExecutorType>
class TaskService {
public:
    explicit TaskService(ExecutorType& executor) : executor_(executor) {}

    /**
     * @brief Process file - works with both executors
     *
     * With Executor: Runs on CPU thread pool
     * With HybridExecutor: Routes IO_BOUND tasks to libuv pool
     */
    auto process_file(const std::string& path) -> void {
        executor_.submit(io_task([path]() {
            std::cout << "[" << get_thread_id() << "] Reading file: " << path << std::endl;
            // File I/O operation
        }));
    }

    /**
     * @brief Process data with priority - works with both executors
     */
    auto process_urgent_data(const std::string& data) -> void {
        executor_.submit(Task{
            [data]() {
                std::cout << "[" << get_thread_id() << "] Processing urgent: " << data << std::endl;
            },
            TaskType::PRIORITIZED,
            Task::Priority::HIGH
        });
    }

    /**
     * @brief Process background computation - works with both executors
     */
    auto process_background(int n) -> void {
        executor_.submit(cpu_task([n]() {
            std::cout << "[" << get_thread_id() << "] Computing fibonacci(" << n << ")" << std::endl;
            volatile uint64_t result = 0;
            // Simulate computation
            for (int i = 0; i < 1000000; ++i) result += i;
        }));
    }

    /**
     * @brief Submit generic task - works with both executors
     */
    auto submit(Task&& task) -> void {
        executor_.submit(std::move(task));
    }

    /**
     * @brief Submit function - works with both executors
     */
    auto submit_func(Task::Func&& func) -> void {
        executor_.submit(std::move(func));
    }

private:
    ExecutorType& executor_;

    static auto get_thread_id() -> std::string {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        return oss.str();
    }
};

// ========================================================================
// Example 1: Same code with different executors
// ========================================================================

void example_same_code_different_executors() {
    std::cout << "\n=== Example 1: Same Code, Different Executors ===\n" << std::endl;

    // Using Executor (CPU thread pool only)
    {
        std::cout << "\n--- Using Executor ---" << std::endl;
        Executor cpu_executor(4);
        TaskService<Executor> service(cpu_executor);

        service.process_file("data.txt");
        service.process_urgent_data("urgent_data");
        service.process_background(40);

        cpu_executor.wait();
    }

    // Using HybridExecutor (intelligent routing)
    {
        std::cout << "\n--- Using HybridExecutor ---" << std::endl;
        EventLoop io_loop;
        HybridExecutor hybrid_executor(io_loop, 4);
        TaskService<HybridExecutor> service(hybrid_executor);

        // Same code, different behavior!
        // IO_BOUND tasks route to libuv, CPU_HEAVY to CPU pool
        service.process_file("data.txt");
        service.process_urgent_data("urgent_data");
        service.process_background(40);

        // Run I/O loop briefly to process libuv tasks
        std::thread io_thread([&] { io_loop.run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        io_loop.stop();
        io_thread.join();
    }
}

// ========================================================================
// Example 2: Unified Task Usage
// ========================================================================

void example_unified_task_usage() {
    std::cout << "\n=== Example 2: Unified Task Usage ===\n" << std::endl;

    Executor executor(2);

    // Using convenience builders from task.hpp
    executor.submit(io_task([] {
        std::cout << "I/O task (Executor ignores type, routes to CPU)" << std::endl;
    }));

    executor.submit(light_cpu_task([] {
        std::cout << "Light CPU task" << std::endl;
    }));

    executor.submit(cpu_task([] {
        std::cout << "Heavy CPU task" << std::endl;
    }));

    executor.submit(with_priority([] {
        std::cout << "High priority task" << std::endl;
    }, Task::Priority::HIGH));

    executor.submit(with_type([] {
        std::cout << "Custom type task" << std::endl;
    }, TaskType::PRIORITIZED));

    executor.wait();
}

// ========================================================================
// Example 3: Polymorphic Executor Interface
// ========================================================================

/**
 * @brief Concept-based executor interface
 *
 * Using concepts to constrain executor types.
 */
template <typename E>
concept ExecutorLike = requires(E& e, Task&& t, Task::Func&& f) {
    { e.submit(std::move(t)) } -> std::same_as<void>;
    { e.submit(std::move(f)) } -> std::same_as<void>;
    { e.num_workers() } -> std::convertible_to<size_t>;
};

/**
 * @brief Generic task processor using concept
 */
template <ExecutorLike E>
auto process_tasks_generic(E& executor) -> void {
    // Submit various tasks
    for (int i = 0; i < 5; ++i) {
        executor.submit(Task{
            [i]() {
                std::cout << "Task " << i << " on thread "
                          << std::this_thread::get_id() << std::endl;
            },
            (i % 2 == 0) ? TaskType::CPU_HEAVY : TaskType::CPU_LIGHT
        });
    }
}

void example_concept_based_interface() {
    std::cout << "\n=== Example 3: Concept-Based Interface ===\n" << std::endl;

    // Works with Executor
    std::cout << "\n--- Executor ---" << std::endl;
    Executor cpu_exec(2);
    process_tasks_generic(cpu_exec);
    cpu_exec.wait();

    // Works with HybridExecutor
    std::cout << "\n--- HybridExecutor ---" << std::endl;
    EventLoop io_loop;
    HybridExecutor hybrid_exec(io_loop, 2);
    process_tasks_generic(hybrid_exec);

    std::thread io_thread([&] { io_loop.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    io_loop.stop();
    io_thread.join();
}

// ========================================================================
// Example 4: Runtime Executor Switching
// ========================================================================

enum class ExecutorChoice {
    CPU_ONLY,
    HYBRID
};

/**
 * @brief Factory function that returns appropriate executor
 *
 * In real code, this would be based on configuration or runtime conditions.
 */
auto create_executor(ExecutorChoice choice, EventLoop& io_loop) -> std::unique_ptr<void> {
    // This is a simplified example - in real code, you might use
    // std::variant or type erasure for true polymorphism
    std::cout << "Creating executor: "
              << (choice == ExecutorChoice::CPU_ONLY ? "CPU_ONLY" : "HYBRID")
              << std::endl;
    return nullptr;
}

void example_runtime_switching() {
    std::cout << "\n=== Example 4: Runtime Switching ===\n" << std::endl;

    EventLoop io_loop;

    // Code that switches executor based on configuration
    auto use_hybrid = true;

    if (use_hybrid) {
        HybridExecutor executor(io_loop, 4);
        std::cout << "Using HybridExecutor" << std::endl;

        // Submit tasks
        executor.submit(io_task([] {
            std::cout << "I/O task (routes to libuv)" << std::endl;
        }));

        executor.submit(cpu_task([] {
            std::cout << "CPU task (routes to CPU pool)" << std::endl;
        }));

        std::thread io_thread([&] { io_loop.run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        io_loop.stop();
        io_thread.join();
    } else {
        Executor executor(4);
        std::cout << "Using Executor (CPU only)" << std::endl;

        // Same task submission code
        executor.submit(io_task([] {
            std::cout << "I/O task (Executor ignores type)" << std::endl;
        }));

        executor.submit(cpu_task([] {
            std::cout << "CPU task" << std::endl;
        }));

        executor.wait();
    }
}

// ========================================================================
// Example 5: Task Priority with Type
// ========================================================================

void example_priority_with_type() {
    std::cout << "\n=== Example 5: Priority with TaskType ===\n" << std::endl;

    Executor executor(2);

    // Submit tasks with both type and priority
    executor.submit(Task{
        [] { std::cout << "CRITICAL I/O task" << std::endl; },
        TaskType::IO_BOUND,
        Task::Priority::CRITICAL
    });

    executor.submit(Task{
        [] { std::cout << "HIGH CPU task" << std::endl; },
        TaskType::CPU_HEAVY,
        Task::Priority::HIGH
    });

    executor.submit(Task{
        [] { std::cout << "NORMAL background task" << std::endl; },
        TaskType::CPU_HEAVY,
        Task::Priority::NORMAL
    });

    executor.submit(Task{
        [] { std::cout << "LOW priority task" << std::endl; },
        TaskType::PRIORITIZED,
        Task::Priority::LOW
    });

    executor.wait();
}

// ========================================================================
// Main
// ========================================================================

int main() {
    std::cout << "MoltCat Unified Executor Interface Examples\n";
    std::cout << "Hardware concurrency: "
              << std::thread::hardware_concurrency() << " threads\n" << std::endl;

    example_same_code_different_executors();
    example_unified_task_usage();
    example_concept_based_interface();
    example_runtime_switching();
    example_priority_with_type();

    std::cout << "\n=== All Examples Complete ===" << std::endl;

    return 0;
}
