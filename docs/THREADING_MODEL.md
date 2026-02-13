# MoltCat Threading Model Design

## 1. Design Principles

### 1.1 Workload Analysis

| Layer | Workload Type | Threading Strategy |
|-------|--------------|-------------------|
| **Gateway** | I/O Bound | libuv Event Loop |
| **Router** | Mixed (Routing + Dispatch) | libuv + Custom Pool |
| **Worker** | CPU Bound | Custom Work-Stealing Pool |
| **TUI** | I/UI Bound | libuv Event Loop |
| **Storage** | I/O Bound | libuv Thread Pool |

### 1.2 Key Insights

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Thread Responsibility                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  ┌─────────────────┐    ┌─────────────────────────────────────────┐    │
│  │  I/O Thread     │    │       CPU Thread Pool                   │    │
│  │  (libuv loop)   │    │    (Work-Stealing)                      │    │
│  ├─────────────────┤    ├─────────────────────────────────────────┤    │
│  │ • HTTP/WebSocket│    │ • Skill Execution                       │    │
│  │ • Event Routing │    │ • Task Processing                       │    │
│  │ • Timer Mgmt    │    │ • Agent Logic                           │    │
│  │ • Async I/O     │    │ • Data Transformation                    │    │
│  └─────────────────┘    └─────────────────────────────────────────┘    │
│                                                                           │
│  Communication via: Lock-free queues (SPSC/MPSC)                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. Threading Architecture

### 2.1 Recommended Approach: Hybrid Model

**Use libuv for I/O + Custom Work-Stealing Pool for CPU work**

```
                    ┌──────────────────────────────────┐
                    │      Application (main thread)    │
                    └──────────────────────────────────┘
                                    │
            ┌───────────────────────┼───────────────────────┐
            │                       │                       │
            ▼                       ▼                       ▼
    ┌───────────────┐      ┌───────────────┐      ┌───────────────┐
    │  I/O Thread   │      │  I/O Thread   │      │  I/O Thread   │
    │  (Gateway)    │      │  (TUI)        │      │  (Internal)   │
    │  libuv loop   │      │  libuv loop   │      │  libuv loop   │
    └───────────────┘      └───────────────┘      └───────────────┘
            │                       │                       │
            └───────────────────────┼───────────────────────┘
                                    │
                                    ▼
                        ┌───────────────────────┐
                        │   Task Dispatcher      │
                        │  (Lock-free Queue)     │
                        └───────────────────────┘
                                    │
                ┌───────────────────┼───────────────────┐
                │                   │                   │
                ▼                   ▼                   ▼
        ┌───────────┐       ┌───────────┐       ┌───────────┐
        │  Worker 1 │       │  Worker 2 │  ...  │  Worker N │
        │ (CPU)     │       │ (CPU)     │       │ (CPU)     │
        └───────────┘       └───────────┘       └───────────┘
                │                   │                   │
                └───────────────────┼───────────────────┘
                                    │
                                    ▼
                        ┌───────────────────────┐
                        │   Result Callback     │
                        │  (Back to I/O thread) │
                        └───────────────────────┘
```

### 2.2 Why Custom Work-Stealing Pool?

| Factor | libuv Thread Pool | Custom Work-Stealing Pool |
|--------|-------------------|--------------------------|
| **Thread Count** | Fixed (UV_THREADPOOL_SIZE) | Dynamic (CPU cores * N) |
| **Work Stealing** | No | Yes |
| **Priority Support** | No | Yes |
| **Load Balancing** | Simple FIFO | Work stealing algorithm |
| **C++20 Integration** | C-style callbacks | Coroutines + futures |
| **Task Metadata** | Limited | Rich (affinity, timeout) |

### 2.3 When to Use Each

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          Decision Matrix                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  Use libuv Thread Pool (uv_queue_work):                                 │
│  ✓ File I/O operations                                                  │
│  ✓ DNS queries                                                          │
│  ✓ Simple blocking operations                                           │
│  ✓ When operation completes, just needs callback                        │
│                                                                           │
│  Use Custom Thread Pool:                                                │
│  ✓ Skill execution (CPU intensive)                                      │
│  ✓ Agent logic processing                                               │
│  ✓ Task orchestration and dependency management                         │
│  ✓ Long-running computations                                            │
│  ✓ When you need work stealing, priorities, or affinity                 │
│                                                                           │
└─────────────────────────────────────────────────────────────────────────┘
```

## 3. Core Scheduling Organization

### 3.1 Source Directory Structure

```
source/
├── core/
│   ├── event_loop.hpp/cpp      # libuv event loop wrapper
│   ├── executor.hpp/cpp        # Work-stealing thread pool
│   ├── scheduler.hpp/cpp       # Task scheduler
│   └── dispatcher.hpp/cpp      # Task dispatcher
├── gateway/
│   └── (uses I/O thread)
├── router/
│   ├── task_router.hpp/cpp     # Routes tasks to workers
│   └── load_balancer.hpp/cpp   # Load balancing strategy
└── worker/
    ├── worker_pool.hpp/cpp     # Worker pool management
    ├── worker_thread.hpp/cpp   # Individual worker
    └── task_queue.hpp/cpp      # Lock-free task queue
```

### 3.2 Component Interactions

```
┌────────────────────────────────────────────────────────────────────────────┐
│                         Core Scheduling Flow                               │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  1. Gateway receives request (I/O Thread)                                  │
│     │                                                                      │
│     ▼                                                                      │
│  2. Router validates and routes task                                       │
│     │                                                                      │
│     ▼                                                                      │
│  3. Dispatcher enqueues to Task Queue                                      │
│     │                                                                      │
│     ▼                                                                      │
│  4. Worker steals from queue (Work-Stealing)                               │
│     │                                                                      │
│     ▼                                                                      │
│  5. Skill executes (CPU Thread)                                            │
│     │                                                                      │
│     ▼                                                                      │
│  6. Result posted to callback queue                                        │
│     │                                                                      │
│     ▼                                                                      │
│  7. I/O Thread sends response                                              │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘
```

## 4. Implementation Recommendations

### 4.1 libuv Integration

```cpp
// core/event_loop.hpp
#pragma once

struct uv_loop_s;
using uv_loop_t = uv_loop_s;

namespace moltcat::core {

/**
 * @brief libuv event loop wrapper
 *
 * Each thread that needs I/O should have its own loop.
 * - Gateway thread: Handles HTTP/WebSocket
 * - TUI thread: Handles terminal I/O
 * - Internal thread: Handles timers and internal async operations
 */
class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // Prevent copy
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // Get raw loop
    auto raw() noexcept -> uv_loop_t* { return loop_; }

    // Run event loop
    auto run() -> void;

    // Stop event loop
    auto stop() -> void;

private:
    uv_loop_t* loop_;
};

} // namespace moltcat::core
```

### 4.2 Custom Thread Pool

```cpp
// core/executor.hpp
#pragma once

#include <coroutine>
#include <functional>
#include <memory>
#include <thread>

namespace moltcat::core {

/**
 * @brief Work-stealing thread pool for CPU-intensive tasks
 *
 * Features:
 * - Work stealing algorithm for load balancing
 * - Priority queue support
 * - C++20 coroutine integration
 * - Thread affinity control
 */
class Executor {
public:
    using Task = std::function<void()>;

    Executor(size_t num_threads = std::thread::hardware_concurrency());
    ~Executor();

    // Submit task to executor
    auto submit(Task&& task) -> void;

    // Submit task with priority
    auto submit_priority(Task&& task, int priority) -> void;

    // Get number of worker threads
    auto num_workers() const noexcept -> size_t { return workers_.size(); }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace moltcat::core
```

### 4.3 Scheduler Design

```cpp
// core/scheduler.hpp
#pragma once

#include "molt_model.hpp"
#include <functional>

namespace moltcat::core {

/**
 * @brief Task scheduler
 *
 * Responsible for:
 * - Managing task queues
 * - Prioritizing tasks
 * - Dispatching to workers
 * - Handling timeouts
 */
class Scheduler {
public:
    using Callback = std::function<void(const MoltResult&)>;

    Scheduler();
    ~Scheduler();

    /**
     * @brief Schedule task for execution
     * @param task Task to execute
     * @param callback Callback when task completes
     */
    auto schedule(const MoltTask& task, Callback callback) -> void;

    /**
     * @brief Schedule delayed task
     * @param task Task to execute
     * @param delay_ms Delay in milliseconds
     * @param callback Callback when task completes
     */
    auto schedule_delayed(const MoltTask& task, uint64_t delay_ms,
                         Callback callback) -> void;

    /**
     * @brief Cancel task
     * @param task_id Task ID to cancel
     * @return true if task was cancelled
     */
    auto cancel(const char* task_id) -> bool;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace moltcat::core
```

## 5. Best Practices

### 5.1 Thread Affinity

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Thread Affinity Guidelines                                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  • I/O Objects (handles, sockets) should remain on their creating thread │
│  • Use uv_async_send() for cross-thread communication                   │
│  • Never share uv handles across threads                                │
│  • Worker threads should NOT touch I/O handles                           │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Lock-Free Communication

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Thread Communication                                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  I/O Thread → Worker Thread:  MPSC Queue (Multi-Producer, Single-Consumer)│
│  Worker Thread → I/O Thread: SPSC Queue (Single-Producer, Single-Consumer)│
│  Worker ↔ Worker:           Work-stealing deques                        │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.3 Coroutine Integration

```cpp
// Example: C++20 coroutine with custom executor
auto execute_task_async(const MoltTask& task) -> Task<MoltResult> {
    // Schedule on executor
    co_await schedule_on(executor_);

    // Execute skill (on worker thread)
    auto result = co_await skill->execute(task, ctx);

    // Result automatically posted back to caller
    co_return result;
}
```

## 6. Summary

### Recommended Configuration for MoltCat

| Component | Thread Type | Count | Purpose |
|-----------|-------------|-------|---------|
| Gateway Thread | libuv event loop | 1 | HTTP/WebSocket I/O |
| TUI Thread | libuv event loop | 1 | Terminal UI I/O |
| Internal Thread | libuv event loop | 1 | Timers, internal async |
| Worker Pool | Work-stealing | CPU cores × 2 | Skill execution |

### Answer to the Original Question

**Do you need an additional async work-stealing thread pool?**

**YES** for MoltCat, because:

1. **Skill execution is CPU-intensive**: libuv's pool is optimized for I/O, not CPU work
2. **Work stealing provides better load balancing**: Especially important for varying task complexities
3. **Priority and timeout support**: Agent tasks often need prioritization
4. **C++20 coroutine integration**: Custom executor enables clean async/await syntax
5. **Scalability**: Can dynamically size based on workload

**libuv should be used for**:
- Network I/O (HTTP/WebSocket)
- File I/O
- Timer management
- Cross-thread signaling (uv_async_t)

**Custom thread pool should be used for**:
- Skill execution
- Agent logic
- Task orchestration
- CPU-intensive data processing
