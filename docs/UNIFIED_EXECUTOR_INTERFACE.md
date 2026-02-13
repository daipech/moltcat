# MoltCat 统一执行器接口设计

## 概述

重构后的执行器系统实现了统一的接口设计，使得上层代码可以在 `Executor` 和 `HybridExecutor` 之间无缝切换，无需修改任务提交代码。

## 核心设计

### 1. 统一的 Task 定义

```cpp
// source/core/task.hpp

class Task {
public:
    enum class Priority : int {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2,
        CRITICAL = 3
    };

    enum class TaskType : int {
        IO_BOUND,       // → libuv pool (in HybridExecutor)
        CPU_LIGHT,      // → libuv pool (in HybridExecutor)
        CPU_HEAVY,      // → CPU pool (in HybridExecutor)
        PRIORITIZED     // → CPU pool (in HybridExecutor)
    };

    using Func = std::function<void()>;

    Func func;                     // 要执行的函数
    TaskType type;                 // 路由提示
    Priority priority;             // 优先级
    uint64_t enqueue_time;         // 入队时间（用于 FIFO）
    int affinity;                  // 线程亲和性
};
```

### 2. 统一的执行器接口

```cpp
// 两个执行器都支持相同的接口方法

class Executor {
    auto submit(Task&& task) -> void;
    auto submit(Task::Func&& func) -> void;
    auto submit_priority(Task::Func&& func, Task::Priority priority) -> void;
    auto submit_affinity(Task&& task, size_t thread_index) -> void;
    auto num_workers() const noexcept -> size_t;
    auto pending_tasks() const noexcept -> size_t;
    auto wait() -> void;
    auto stop() -> void;
};

class HybridExecutor {
    // 相同的接口
    auto submit(Task&& task) -> void;
    auto submit(Task::Func&& func) -> void;
    auto submit_priority(Task::Func&& func, Task::Priority priority) -> void;
    auto submit_affinity(Task&& task, size_t thread_index) -> void;
    auto num_workers() const noexcept -> size_t;
    auto pending_tasks() const noexcept -> size_t;
    auto wait() -> void;
    auto stop() -> void;

    // 额外的显式路由方法
    auto submit_libuv(Task&& task) -> void;
    auto submit_cpu(Task&& task) -> void;
};
```

## 文件结构

```
source/core/
├── task.hpp              # 统一的 Task 定义
├── executor.hpp          # CPU 线程池执行器
├── hybrid_executor.hpp   # 混合执行器（libuv + CPU）
└── event_loop.hpp        # libuv 事件循环封装
```

## 使用方式

### 基本用法（完全相同）

```cpp
// 使用 Executor
Executor executor(4);
executor.submit(Task{[] { do_work(); }});

// 使用 HybridExecutor（只需修改类型）
EventLoop io_loop;
HybridExecutor executor(io_loop, 4);
executor.submit(Task{[] { do_work(); }});  // 相同的代码！
```

### 带优先级的任务

```cpp
// 两个执行器都支持
executor.submit(Task{
    [] { urgent_work(); },
    Task::Priority::HIGH
});

// 或者使用便捷函数
executor.submit_priority(
    [] { urgent_work(); },
    Task::Priority::HIGH
);
```

### 带类型提示的任务

```cpp
// TaskType 在 Executor 中被忽略，在 HybridExecutor 中用于路由
executor.submit(Task{
    [] { do_io(); },
    TaskType::IO_BOUND
});

// 使用便捷构造函数
executor.submit(io_task([] { do_io(); }));
executor.submit(cpu_task([] { compute(); }));
executor.submit(light_cpu_task([] { quick(); }));
```

### 便捷构造函数（task.hpp）

```cpp
// 类型构造器
io_task(func)           // TaskType::IO_BOUND
light_cpu_task(func)    // TaskType::CPU_LIGHT
cpu_task(func)          // TaskType::CPU_HEAVY
prioritized_task(func, priority)  // TaskType::PRIORITIZED + priority

// 通用构造器
with_type(func, type)           // 指定 TaskType
with_priority(func, priority)   // 指定 Priority
task_with(func, type, priority) // 同时指定两者
```

## 执行器行为差异

| 特性 | Executor | HybridExecutor |
|------|----------|----------------|
| **IO_BOUND** | CPU 线程池 | libuv 线程池 |
| **CPU_LIGHT** | CPU 线程池 | libuv 线程池 |
| **CPU_HEAVY** | CPU 线程池 | CPU 线程池 |
| **PRIORITIZED** | CPU 线程池 | CPU 线程池 |
| **submit_libuv()** | 不支持 | libuv 线程池 |
| **submit_cpu()** | 不支持 | CPU 线程池 |

## 上层代码示例

### 模板化服务（无需修改）

```cpp
template <typename ExecutorType>
class TaskService {
    ExecutorType& executor_;

public:
    auto process_file(const std::string& path) -> void {
        // 相同的代码，两个执行器都支持
        executor_.submit(io_task([path]() {
            // 文件操作
        }));
    }

    auto process_urgent_data(const std::string& data) -> void {
        executor_.submit(Task{
            [data]() { /* ... */ },
            TaskType::PRIORITIZED,
            Task::Priority::HIGH
        });
    }
};

// 使用 Executor
Executor cpu_exec(4);
TaskService<Executor> service1(cpu_exec);

// 使用 HybridExecutor（只需修改类型）
EventLoop io_loop;
HybridExecutor hybrid_exec(io_loop, 4);
TaskService<HybridExecutor> service2(hybrid_exec);
```

### Concept 约束

```cpp
template <typename E>
concept ExecutorLike = requires(E& e, Task&& t, Task::Func&& f) {
    { e.submit(std::move(t)) } -> std::same_as<void>;
    { e.submit(std::move(f)) } -> std::same_as<void>;
    { e.num_workers() } -> std::convertible_to<size_t>;
};

template <ExecutorLike E>
auto process_tasks(E& executor) -> void {
    executor.submit(Task{[] { /* ... */ }});
}
```

## 迁移指南

### 从旧代码迁移

**旧代码（使用 std::function）：**
```cpp
executor.submit([] { work(); });
```

**新代码（使用统一的 Task）：**
```cpp
// 方式 1：直接传递函数（自动转换）
executor.submit([] { work(); });

// 方式 2：显式使用 Task
executor.submit(Task{[] { work(); }});

// 方式 3：指定类型或优先级
executor.submit(cpu_task([] { work(); }));
executor.submit(with_priority([] { work(); }, Task::Priority::HIGH));
```

### 选择执行器

**使用 Executor 的场景：**
- 纯 CPU 密集型任务
- 不需要文件 I/O
- 需要工作窃取负载均衡
- 简单的部署环境

**使用 HybridExecutor 的场景：**
- 混合 I/O 和 CPU 任务
- 需要利用 libuv 的文件 I/O 优化
- 需要区分轻量级和重量级任务
- 希望自动路由到合适的线程池

## 构建示例

```bash
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake

# 构建统一接口示例
cmake --build . --target unified_executor_example

# 运行
./examples/unified_executor_example
```

## 总结

统一接口设计的优势：

1. **零成本抽象**：上层代码无需修改即可切换执行器
2. **类型安全**：编译期检查，减少运行时错误
3. **灵活路由**：TaskType 提供路由提示，优先级提供调度控制
4. **便捷构造**：提供多种便捷函数简化任务创建
5. **向后兼容**：保留函数直接提交的方式
