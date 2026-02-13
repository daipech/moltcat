/**
 * @file threading_example.cpp
 * @brief Example demonstrating MoltCat threading model
 *
 * This example shows how to use EventLoop, Executor, and Scheduler together
 * to build a scalable multi-threaded application.
 */

#include <moltcat/core/event_loop.hpp>
#include <moltcat/core/executor.hpp>
#include <moltcat/core/scheduler.hpp>
#include <moltcat/molt_model.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using namespace moltcat::core;
using namespace moltcat;

// ========================================================================
// Example Skill Implementation
// ========================================================================

/**
 * @brief Example skill that simulates CPU-intensive work
 */
class ComputeSkill : public IMoltSkill {
public:
    auto name() const -> const char* override { return "ComputeSkill"; }
    auto version() const -> const char* override { return "1.0.0"; }
    auto description() const -> const char* override { return "CPU-intensive compute skill"; }

    auto supported_task_types() -> IList<IString*>* override {
        auto list = IList<IString*>::create();
        list->add(IString::create("fibonacci"));
        list->add(IString::create("prime"));
        return list;
    }

    auto can_handle(const char* task_type) const -> bool override {
        return std::strcmp(task_type, "fibonacci") == 0 ||
               std::strcmp(task_type, "prime") == 0;
    }

    auto execute(const MoltTask& task, MoltContext& ctx) -> MoltResult override {
        (void)ctx;
        MoltResult result;

        const char* task_type = task.get_type();
        const char* payload = task.get_payload();

        // Parse number from payload: {"n": 30}
        int n = 0;
        std::sscanf(payload, "{\"n\": %d}", &n);

        std::cout << "[" << std::this_thread::get_id() << "] "
                  << "Executing " << task_type << " with n=" << n << std::endl;

        if (std::strcmp(task_type, "fibonacci") == 0) {
            // Simulate CPU work
            auto value = fibonacci(n);
            char buffer[128];
            std::snprintf(buffer, sizeof(buffer), "{\"result\": %llu}", value);
            result.data = IString::create(buffer);
        } else if (std::strcmp(task_type, "prime") == 0) {
            auto value = nth_prime(n);
            char buffer[128];
            std::snprintf(buffer, sizeof(buffer), "{\"result\": %llu}", value);
            result.data = IString::create(buffer);
        }

        result.success = true;
        result.status = TaskStatus::COMPLETED;

        std::cout << "[" << std::this_thread::get_id() << "] "
                  << "Completed " << task_type << std::endl;

        return result;
    }

private:
    static auto fibonacci(int n) -> uint64_t {
        if (n <= 1) return n;
        uint64_t a = 0, b = 1;
        for (int i = 2; i <= n; ++i) {
            uint64_t c = a + b;
            a = b;
            b = c;
        }
        return b;
    }

    static auto nth_prime(int n) -> uint64_t {
        if (n <= 0) return 2;
        int count = 0;
        uint64_t num = 1;
        while (count < n) {
            ++num;
            bool is_prime = true;
            for (uint64_t i = 2; i * i <= num; ++i) {
                if (num % i == 0) {
                    is_prime = false;
                    break;
                }
            }
            if (is_prime) ++count;
        }
        return num;
    }
};

// ========================================================================
// Example: Basic Executor Usage
// ========================================================================

void example_executor() {
    std::cout << "\n=== Example 1: Basic Executor ===\n" << std::endl;

    // Create executor with 4 worker threads
    Executor executor(4);

    // Submit tasks
    for (int i = 0; i < 8; ++i) {
        executor.submit([i] {
            std::cout << "[" << std::this_thread::get_id() << "] "
                      << "Task " << i << " executing" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }

    // Wait for all tasks to complete
    executor.wait();
    std::cout << "All tasks completed" << std::endl;
}

// ========================================================================
// Example: Event Loop with Timer
// ========================================================================

void example_event_loop() {
    std::cout << "\n=== Example 2: Event Loop with Timer ===\n" << std::endl;

    EventLoop loop;

    // Create timer
    auto timer = loop.create_timer([](void* data) {
        auto count = static_cast<int*>(data);
        std::cout << "Timer fired, count = " << *count << std::endl;
        (*count)++;
    });

    int count = 0;
    timer->raw()->data = &count;

    // Start timer: 500ms interval, repeat
    timer->start(500, 500);

    // Run event loop in separate thread
    std::thread loop_thread([&] {
        std::cout << "Event loop running on thread "
                  << std::this_thread::get_id() << std::endl;
        loop.run();
    });

    // Let it run for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Stop loop
    loop.stop();
    loop_thread.join();

    std::cout << "Timer fired " << count << " times" << std::endl;
}

// ========================================================================
// Example: Scheduler with Callback
// ========================================================================

void example_scheduler_callback() {
    std::cout << "\n=== Example 3: Scheduler with Callback ===\n" << std::endl;

    // I/O event loop
    EventLoop io_loop;

    // CPU executor
    Executor executor(4);

    // Scheduler
    Scheduler scheduler(io_loop, executor);

    // Create task
    MoltTask task;
    task.id = IString::create("task-001");
    task.type = IString::create("fibonacci");
    task.payload = IString::create("{\"n\": 40}");
    task.priority = TaskPriority::NORMAL;

    // Schedule with callback
    scheduler.schedule(task, [](const MoltResult& result) {
        std::cout << "[" << std::this_thread::get_id() << "] "
                  << "Callback on I/O thread" << std::endl;
        if (result.success) {
            std::cout << "Result: " << result.get_data() << std::endl;
        }
    });

    // Run I/O loop in separate thread
    std::thread io_thread([&] {
        std::cout << "I/O loop running on thread "
                  << std::this_thread::get_id() << std::endl;
        io_loop.run();
    });

    // Wait a bit for task to complete
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Cleanup
    io_loop.stop();
    io_thread.join();

    task.id->destroy();
    task.type->destroy();
    task.payload->destroy();
}

// ========================================================================
// Example: Scheduler with Future
// ========================================================================

void example_scheduler_future() {
    std::cout << "\n=== Example 4: Scheduler with Future ===\n" << std::endl;

    EventLoop io_loop;
    Executor executor(4);
    Scheduler scheduler(io_loop, executor);

    // Create task
    MoltTask task;
    task.id = IString::create("task-002");
    task.type = IString::create("prime");
    task.payload = IString::create("{\"n\": 10000}");
    task.priority = TaskPriority::NORMAL;

    // Schedule and get future
    auto future = schedule_future(scheduler, task);

    // Run I/O loop in separate thread
    std::thread io_thread([&] { io_loop.run(); });

    // Wait for result (blocking)
    auto result = future.get();

    std::cout << "Future resolved: " << result.get_data() << std::endl;

    // Cleanup
    io_loop.stop();
    io_thread.join();

    task.id->destroy();
    task.type->destroy();
    task.payload->destroy();
}

// ========================================================================
// Example: Priority Tasks
// ========================================================================

void example_priority() {
    std::cout << "\n=== Example 5: Priority Tasks ===\n" << std::endl;

    Executor executor(2);

    // Submit mixed priority tasks
    for (int i = 0; i < 10; ++i) {
        auto priority = (i % 3 == 0) ? Task::Priority::HIGH : Task::Priority::NORMAL;
        executor.submit_priority([i] {
            std::cout << "Task " << i << " executing" << std::endl;
        }, priority);
    }

    executor.wait();
}

// ========================================================================
// Main
// ========================================================================

int main() {
    std::cout << "MoltCat Threading Model Examples\n";
    std::cout << "Hardware concurrency: "
              << std::thread::hardware_concurrency() << " threads\n" << std::endl;

    example_executor();
    example_event_loop();
    example_scheduler_callback();
    example_scheduler_future();
    example_priority();

    std::cout << "\n=== All Examples Complete ===" << std::endl;
    return 0;
}
