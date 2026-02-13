# MoltCat 线程池复用分析

## 问题：能否复用 libuv 的线程池作为 CPU Thread Pool？

## 1. libuv 线程池机制分析

### 1.1 内部实现

```
libuv Thread Pool (默认 4 线程)
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                           │
│  uv_queue_work()                                                          │
│       │                                                                   │
│       ▼                                                                   │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    Global Work Queue (FIFO)                      │   │
│  │  [File I/O Task] [DNS Query] [User Work] [File I/O Task] ...     │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                               │                                           │
│                               ▼                                           │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐    │
│  │  Thread 1   │  │  Thread 2   │  │  Thread 3   │  │  Thread 4   │    │
│  │  (Worker)   │  │  (Worker)   │  │  (Worker)   │  │  (Worker)   │    │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘    │
│         │                │                │                │            │
│         └────────────────┴────────────────┴────────────────┘            │
│                           │                                               │
│                           ▼                                               │
│                    after_work_cb()                                        │
│                    (回调到事件循环线程)                                    │
│                                                                           │
└───────────────────────────────────────────────────────────────────────────┘
```

### 1.2 API 接口

```cpp
// libuv 提供的工作队列 API
typedef void (*uv_work_cb)(uv_work_t* req);
typedef void (*uv_after_work_cb)(uv_work_t* req, int status);

// 提交工作到线程池
int uv_queue_work(
    uv_loop_t* loop,        // 事件循环
    uv_work_t* req,         // 请求对象
    uv_work_cb work_cb,     // 在线程池中执行的函数
    uv_after_work_cb after_work_cb  // 完成后的回调（在事件循环线程执行）
);
```

### 1.3 特性限制

| 特性 | libuv 线程池 | 自定义线程池 |
|------|-------------|-------------|
| 线程数量 | 固定 (环境变量) | 动态调整 |
| 任务优先级 | ❌ 无 | ✅ 支持 |
| 工作窃取 | ❌ 无 | ✅ 支持 |
| 负载均衡 | FIFO 队列 | 工作窃取算法 |
| CPU 密集型优化 | ❌ 针对阻塞 I/O | ✅ 针对计算任务 |
| 线程亲和性 | ❌ 无 | ✅ 可控 |
| 取消任务 | ❌ 不支持 | ✅ 支持 |
| 超时控制 | ❌ 无 | ✅ 支持 |

## 2. 复用可行性分析

### 2.1 技术上可行

```cpp
// 示例：使用 libuv 线程池执行 CPU 任务
struct CPUWork {
    uv_work_t req;
    std::function<void()> work;
    std::function<void()> callback;
};

auto execute_cpu_task(uv_loop_t* loop,
                      std::function<void()> work,
                      std::function<void()> callback) -> void {
    auto cpu_work = new CPUWork();
    cpu_work->req.data = cpu_work;
    cpu_work->work = std::move(work);
    cpu_work->callback = std::move(callback);

    uv_queue_work(loop, &cpu_work->req,
        [](uv_work_t* req) {
            // 在线程池中执行（CPU 密集型）
            auto w = static_cast<CPUWork*>(req->data);
            w->work();
        },
        [](uv_work_t* req, int status) {
            // 在事件循环线程执行（回调）
            auto w = static_cast<CPUWork*>(req->data);
            w->callback();
            delete w;
        }
    );
}
```

### 2.2 优缺点对比

#### ✅ 复用的优点

| 优点 | 说明 |
|------|------|
| **减少线程数量** | 避免线程过多导致上下文切换开销 |
| **简化架构** | 统一的线程管理，减少代码复杂度 |
| **降低内存** | 每个线程占用栈空间（通常 1-8MB） |
| **适合轻量级任务** | 短暂的 CPU 任务可以很好地处理 |
| **内置支持** | 无需额外实现线程池 |

#### ❌ 复用的缺点

| 缺点 | 影响 |
|------|------|
| **文件 I/O 竞争** | CPU 密集型任务会阻塞文件 I/O 操作 |
| **无优先级** | 智能体任务通常需要优先级调度 |
| **无工作窃取** | 负载不均衡时，某些线程空闲而其他线程忙碌 |
| **不可取消** | 任务提交后无法中途取消 |
| **无超时控制** | 无法对长时间运行的任务设置超时 |
| **线程数固定** | 无法根据负载动态调整 |
| **混合负载问题** | I/O 和 CPU 任务混用影响性能 |

## 3. 性能影响分析

### 3.1 I/O 与 CPU 混合场景

```
场景：同时进行大量文件 I/O 和 CPU 密集型计算

仅使用 libuv 线程池：
┌─────────────────────────────────────────────────────────────────────────┐
│  Thread 1: [读文件 → 等待 I/O] → 执行 Fibonacci → [读文件 → 等待 I/O]  │
│  Thread 2: [读文件 → 等待 I/O] → 执行 Fibonacci → [读文件 → 等待 I/O]  │
│  Thread 3: [读文件 → 等待 I/O] → 执行 Fibonacci → [读文件 → 等待 I/O]  │
│  Thread 4: [读文件 → 等待 I/O] → 执行 Fibonacci → [读文件 → 等待 I/O]  │
│                                                                           │
│  问题：                                                                   │
│  - CPU 任务阻塞了文件 I/O 的处理                                        │
│  - 文件 I/O 等待时，CPU 线程空闲                                         │
│  - 总吞吐量降低                                                           │
└───────────────────────────────────────────────────────────────────────────┘

分离的线程池：
┌─────────────────────────────────────────────────────────────────────────┐
│  libuv 线程池:                    CPU 线程池:                            │
│  Thread 1: [读文件] [读文件] [读文件]    Thread 1: [Fibonacci] [Prime]    │
│  Thread 2: [读文件] [读文件] [读文件]    Thread 2: [Fibonacci] [Prime]    │
│  Thread 3: [读文件] [读文件] [读文件]    Thread 3: [Fibonacci] [Prime]    │
│  Thread 4: [读文件] [读文件] [读文件]    Thread 4: [Fibonacci] [Prime]    │
│                                                                           │
│  优势：                                                                   │
│  - I/O 和 CPU 任务并行处理                                              │
│  - 各自优化                                                              │
│  - 更高的总吞吐量                                                        │
└───────────────────────────────────────────────────────────────────────────┘
```

### 3.2 基准测试建议

```cpp
// 建议的对比测试
void benchmark_compare() {
    const int num_tasks = 1000;

    // 测试 1: 仅 libuv 线程池
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_tasks; ++i) {
        uv_queue_work(loop, ...);  // CPU 密集型任务
    }
    auto libuv_only = measure_time();

    // 测试 2: 混合使用
    // 文件 I/O → libuv 线程池
    // CPU 任务 → 独立线程池
    auto hybrid = measure_time();

    // 测试 3: 自定义线程池
    auto custom_pool = measure_time();
}
```

## 4. 混合策略建议

### 4.1 推荐方案：分类使用

```
┌────────────────────────────────────────────────────────────────────────────┐
│                          Task Classification                                │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  使用 libuv 线程池 (uv_queue_work):                                         │
│  ✓ 文件读写操作                                                            │
│  ✓ DNS 查询                                                                │
│  ✓ 简单的数据转换 (< 1ms)                                                  │
│  ✓ 轻量级加密/解密                                                          │
│                                                                              │
│  使用自定义 CPU 线程池:                                                     │
│  ✓ 智能体技能执行 (> 10ms)                                                 │
│  ✓ 复杂计算任务                                                            │
│  ✓ 需要优先级调度的任务                                                    │
│  ✓ 需要超时控制的任务                                                      │
│  ✓ 需要取消能力的任务                                                      │
│                                                                              │
└────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 两级调度策略

```
┌────────────────────────────────────────────────────────────────────────────┐
│                         Two-Level Scheduler                                 │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │                       Task Dispatcher                               │    │
│  │                                                                      │    │
│  │  ┌─────────────────┐          ┌─────────────────┐                   │    │
│  │  │  Task Analyzer  │          │ Load Monitor    │                   │    │
│  │  └────────┬────────┘          └────────┬────────┘                   │    │
│  │           │                             │                            │    │
│  │           ▼                             │                            │    │
│  │  ┌─────────────────┐                   │                            │    │
│  │  │  Classify:      │                   │                            │    │
│  │  │  • I/O bound?   │                   │                            │    │
│  │  │  • CPU bound?   │                   │                            │    │
│  │  │  • Duration?    │                   │                            │    │
│  │  │  • Priority?    │                   │                            │    │
│  │  └────────┬────────┘                   │                            │    │
│  └───────────┼────────────────────────────┼────────────────────────────┘    │
│              │                            │                                 │
│      ┌───────┴────────┐          ┌────────┴───────┐                       │
│      ▼                ▼          ▼                ▼                       │
│  ┌─────────┐    ┌─────────┐  ┌─────────┐    ┌─────────┐                   │
│  │ libuv   │    │ Custom  │  │ libuv   │    │ Custom  │                   │
│  │ Pool    │    │ Pool    │  │ Pool    │    │ Pool    │                   │
│  │(I/O)    │    │ (CPU)   │  │(I/O)    │    │ (CPU)   │                   │
│  └─────────┘    └─────────┘  └─────────┘    └─────────┘                   │
│                                                                              │
│  决策条件:                                                                  │
│  1. 任务类型: I/O 密集 → libuv, CPU 密集 → Custom                           │
│  2. 预计耗时: < 1ms → libuv, > 10ms → Custom                               │
│  3. 优先级要求: 需要优先级 → Custom                                         │
│  4. 系统负载: CPU 高负载 → 减少 Custom 任务数                               │
│                                                                              │
└────────────────────────────────────────────────────────────────────────────┘
```

### 4.3 实现代码

```cpp
// core/hybrid_executor.hpp
#pragma once

#include "event_loop.hpp"
#include "executor.hpp"
#include <functional>

namespace moltcat::core {

/**
 * @brief Hybrid executor that intelligently routes tasks
 *
 * Routes tasks to libuv thread pool or custom thread pool
 * based on task characteristics and system load.
 */
class HybridExecutor {
public:
    using Task = std::function<void()>;

    enum class TaskType {
        IO_BOUND,       // File I/O, DNS (→ libuv)
        CPU_LIGHT,      // Quick computation (< 1ms) (→ libuv)
        CPU_HEAVY,      // Heavy computation (> 10ms) (→ custom)
        PRIORITIZED     // Requires priority (→ custom)
    };

    HybridExecutor(EventLoop& io_loop, size_t cpu_threads = 0);

    /**
     * @brief Submit task with automatic routing
     */
    auto submit(Task&& task, TaskType type = TaskType::CPU_HEAVY) -> void;

    /**
     * @brief Submit task to libuv thread pool
     */
    auto submit_libuv(Task&& task) -> void;

    /**
     * @brief Submit task to custom CPU thread pool
     */
    auto submit_cpu(Task&& task) -> void;

    /**
     * @brief Submit prioritized task
     */
    auto submit_priority(Task&& task, Task::Priority priority) -> void;

    /**
     * @brief Get current load statistics
     */
    struct LoadStats {
        size_t libuv_pending = 0;
        size_t cpu_pending = 0;
        size_t cpu_active = 0;
        double cpu_utilization = 0.0;
    };

    [[nodiscard]] auto get_stats() const -> LoadStats;

private:
    EventLoop& io_loop_;
    Executor cpu_executor_;

    // Load tracking
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> libuv_pending_{0};
};

} // namespace moltcat::core
```

## 5. 总结与建议

### 5.1 答案

**技术上可行，但需要根据场景选择。**

对于 MoltCat 智能体协作平台：

| 阶段 | 推荐方案 | 理由 |
|------|---------|------|
| **MVP/原型** | 复用 libuv | 快速开发，减少复杂度 |
| **生产环境** | 分离线程池 | 更好的性能和可控性 |
| **混合方案** | 分类路由 | 平衡开发成本和性能 |

### 5.2 决策树

```
是否需要独立 CPU 线程池？
│
├─ 任务执行时间 > 10ms？ → YES → 独立线程池
│
├─ 需要任务优先级？ → YES → 独立线程池
│
├─ 需要任务取消？ → YES → 独立线程池
│
├─ 需要超时控制？ → YES → 独立线程池
│
├─ 需要工作窃取负载均衡？ → YES → 独立线程池
│
└─ 大量文件 I/O 混合 CPU 任务？ → YES → 分离线程池

如果以上都是 NO，可以复用 libuv 线程池
```

### 5.3 实施建议

**阶段 1: 快速原型**
```cpp
// 使用 libuv 线程池
uv_queue_work(loop, &req, work_cb, after_cb);
```

**阶段 2: 性能优化**
```cpp
// 添加简单包装，便于后续切换
auto execute_async(Task&& task) -> void {
#ifdef USE_LIBUV_POOL
    uv_queue_work(...);
#else
    custom_executor.submit(std::move(task));
#endif
}
```

**阶段 3: 生产部署**
```cpp
// 使用混合调度器
HybridExecutor executor(io_loop);
executor.submit(task, TaskType::CPU_HEAVY);
```

### 5.4 配置建议

```cpp
// 推荐的线程配置
struct ThreadingConfig {
    // libuv 线程池 (环境变量或代码设置)
    size_t libuv_threads = 4;  // UV_THREADPOOL_SIZE

    // 自定义 CPU 线程池
    size_t cpu_threads = std::thread::hardware_concurrency();

    // I/O 事件循环线程
    size_t io_loops = 3;  // Gateway, TUI, Internal

    // 总线程数
    size_t total() const { return libuv_threads + cpu_threads + io_loops; }
};

// 对于 8 核 CPU:
// libuv: 4 线程
// CPU Pool: 16 线程 (cores × 2)
// I/O Loops: 3 线程
// 总计: 23 线程
```
