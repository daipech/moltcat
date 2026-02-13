/**
 * @file hybrid_executor_example.cpp
 * @brief Example demonstrating HybridExecutor usage
 *
 * This example shows:
 * 1. Automatic task routing based on type
 * 2. Explicit routing to libuv vs CPU pool
 * 3. Performance comparison
 * 4. Practical usage patterns
 */

#include <moltcat/core/hybrid_executor.hpp>
#include <moltcat/core/event_loop.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <cstring>
#include <random>

using namespace moltcat::core;

// ========================================================================
// Helper Functions
// ========================================================================

auto get_thread_id() -> std::string {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

// ========================================================================
// Example 1: Automatic Task Routing
// ========================================================================

void example_automatic_routing() {
    std::cout << "\n=== Example 1: Automatic Task Routing ===\n" << std::endl;

    EventLoop io_loop;
    HybridExecutor executor(io_loop);

    std::atomic<int> completed{0};

    // I/O-bound task → routes to libuv pool
    executor.submit(
        [&completed]() {
            std::cout << "[" << get_thread_id() << "] "
                      << "File I/O task (libuv pool)" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            completed++;
        },
        HybridExecutor::TaskType::IO_BOUND
    );

    // Light CPU task → routes to libuv pool
    executor.submit(
        [&completed]() {
            std::cout << "[" << get_thread_id() << "] "
                      << "Light CPU task (libuv pool)" << std::endl;
            volatile int x = 0;
            for (int i = 0; i < 1000; ++i) x += i;
            completed++;
        },
        HybridExecutor::TaskType::CPU_LIGHT
    );

    // Heavy CPU task → routes to CPU pool
    executor.submit(
        [&completed]() {
            std::cout << "[" << get_thread_id() << "] "
                      << "Heavy CPU task (CPU pool)" << std::endl;
            volatile int x = 0;
            for (int i = 0; i < 10000000; ++i) x += i;
            completed++;
        },
        HybridExecutor::TaskType::CPU_HEAVY
    );

    // Wait for completion
    while (completed.load() < 3) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "All tasks completed: " << completed << std::endl;
}

// ========================================================================
// Example 2: Explicit Routing
// ========================================================================

void example_explicit_routing() {
    std::cout << "\n=== Example 2: Explicit Routing ===\n" << std::endl;

    EventLoop io_loop;
    HybridExecutor executor(io_loop);

    std::atomic<int> completed{0};

    // Explicitly route to libuv pool (for I/O)
    executor.submit_libuv([&completed]() {
        std::cout << "[" << get_thread_id() << "] "
                  << "Reading file (libuv pool)" << std::endl;

        // Simulate file I/O
        std::ofstream("test_temp.txt") << "Hello, World!";
        std::ifstream file("test_temp.txt");
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        std::cout << "File content: " << content << std::endl;
        std::remove("test_temp.txt");

        completed++;
    });

    // Explicitly route to CPU pool (for computation)
    executor.submit_cpu([&completed]() {
        std::cout << "[" << get_thread_id() << "] "
                  << "Computing Fibonacci (CPU pool)" << std::endl;

        auto fib = [](int n) -> uint64_t {
            if (n <= 1) return n;
            uint64_t a = 0, b = 1;
            for (int i = 2; i <= n; ++i) {
                uint64_t c = a + b;
                a = b;
                b = c;
            }
            return b;
        };

        auto result = fib(40);
        std::cout << "Fibonacci(40) = " << result << std::endl;

        completed++;
    });

    // Wait for completion
    while (completed.load() < 2) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "All tasks completed: " << completed << std::endl;
}

// ========================================================================
// Example 3: Priority Tasks
// ========================================================================

void example_priority_tasks() {
    std::cout << "\n=== Example 3: Priority Tasks ===\n" << std::endl;

    EventLoop io_loop;
    HybridExecutor executor(io_loop);

    std::atomic<int> completed{0};

    // Submit tasks with different priorities
    for (int i = 0; i < 10; ++i) {
        auto priority = (i % 3 == 0) ? Task::Priority::HIGH :
                       (i % 3 == 1) ? Task::Priority::NORMAL :
                                      Task::Priority::LOW;

        executor.submit_priority(
            [i, &completed]() {
                std::cout << "Task " << i << " executing" << std::endl;
                completed++;
            },
            priority
        );
    }

    executor.wait();
    std::cout << "All priority tasks completed: " << completed << std::endl;
}

// ========================================================================
// Example 4: File I/O Helper
// ========================================================================

void example_file_io_helper() {
    std::cout << "\n=== Example 4: File I/O Helper ===\n" << std::endl;

    EventLoop io_loop;
    HybridExecutor executor(io_loop);

    // Create test file
    {
        std::ofstream file("test_data.txt");
        file << "Line 1\nLine 2\nLine 3\n";
    }

    std::atomic<bool> done{false};

    submit_file_io(executor, "test_data.txt",
        [&done](const std::string& content) {
            std::cout << "File content:\n" << content << std::endl;
            done = true;
        }
    );

    while (!done.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::remove("test_data.txt");
}

// ========================================================================
// Example 5: Compute Helper
// ========================================================================

void example_compute_helper() {
    std::cout << "\n=== Example 5: Compute Helper ===\n" << std::endl;

    EventLoop io_loop;
    HybridExecutor executor(io_loop);

    std::atomic<bool> done{false};

    submit_compute(executor,
        [](int n) -> uint64_t {
            uint64_t result = 1;
            for (int i = 1; i <= n; ++i) {
                result *= i;
            }
            return result;
        },
        20,  // Compute 20!
        [&done](const uint64_t& result) {
            std::cout << "20! = " << result << std::endl;
            done = true;
        }
    );

    while (!done.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ========================================================================
// Example 6: Load Statistics
// ========================================================================

void example_load_stats() {
    std::cout << "\n=== Example 6: Load Statistics ===\n" << std::endl;

    EventLoop io_loop;
    HybridExecutor executor(io_loop);

    // Submit many tasks
    for (int i = 0; i < 20; ++i) {
        if (i % 2 == 0) {
            executor.submit_libuv([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            });
        } else {
            executor.submit_cpu([] {
                volatile int x = 0;
                for (int j = 0; j < 1000000; ++j) x += j;
            });
        }
    }

    // Check stats
    auto stats = executor.get_stats();
    std::cout << "Statistics:" << std::endl;
    std::cout << "  libuv pending: " << stats.libuv_pending << std::endl;
    std::cout << "  CPU pending: " << stats.cpu_pending << std::endl;
    std::cout << "  CPU workers: " << stats.cpu_active << std::endl;

    executor.wait();
    std::cout << "All tasks completed" << std::endl;
}

// ========================================================================
// Example 7: Mixed Workload Comparison
// ========================================================================

void example_mixed_workload_comparison() {
    std::cout << "\n=== Example 7: Mixed Workload ===\n" << std::endl;

    const int num_iterations = 50;

    // Test with mixed I/O and CPU tasks
    EventLoop io_loop;
    HybridExecutor executor(io_loop);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
        if (i % 3 == 0) {
            // I/O task → libuv pool
            executor.submit_libuv([i]() {
                std::ofstream("temp_" + std::to_string(i) + ".txt") << "data";
                std::string content;
                std::ifstream("temp_" + std::to_string(i) + ".txt")
                    >> content;
                std::remove(("temp_" + std::to_string(i) + ".txt").c_str());
            });
        } else {
            // CPU task → CPU pool
            executor.submit_cpu([i]() {
                volatile int x = 0;
                for (int j = 0; j < 100000; ++j) x += j;
            });
        }
    }

    executor.wait();

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Mixed workload completed in " << elapsed.count() << "ms" << std::endl;
    std::cout << "  I/O tasks: ~" << (num_iterations / 3) << std::endl;
    std::cout << "  CPU tasks: ~" << (2 * num_iterations / 3) << std::endl;
}

// ========================================================================
// Main
// ========================================================================

int main() {
    std::cout << "MoltCat HybridExecutor Examples\n";
    std::cout << "Hardware concurrency: "
              << std::thread::hardware_concurrency() << " threads\n" << std::endl;

    example_automatic_routing();
    example_explicit_routing();
    example_priority_tasks();
    example_file_io_helper();
    example_compute_helper();
    example_load_stats();
    example_mixed_workload_comparison();

    std::cout << "\n=== All Examples Complete ===" << std::endl;

    return 0;
}
