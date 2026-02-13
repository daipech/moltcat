# MoltCat 统一调度器接口设计

## 概述

重构后的调度器系统实现了灵活的多态设计，支持：
1. **模板化调度器** - 编译期类型安全
2. **抽象接口** - 运行时多态
3. **类型擦除包装** - 灵活的运行时切换

## 核心组件

### 1. IScheduler - 抽象接口

```cpp
class IScheduler {
public:
    virtual auto schedule(const MoltTask& task, Callback callback) -> bool = 0;
    virtual auto schedule_delayed(const MoltTask& task, uint64_t delay_ms, Callback callback) -> bool = 0;
    virtual auto schedule_with_timeout(const MoltTask& task, uint64_t timeout_ms, Callback callback) -> bool = 0;
    virtual auto cancel(const char* task_id) -> bool = 0;
    virtual auto get_status(const char* task_id) const -> TaskStatus = 0;
    virtual auto get_stats() const -> Stats = 0;
    virtual auto shutdown(bool wait_for_completion = true) -> void = 0;
    virtual auto is_running() const -> bool = 0;
};
```

### 2. Scheduler<ExecutorType> - 模板实现

```cpp
template <typename ExecutorType>
class Scheduler : public IScheduler {
    // 实现 IScheduler 接口
    // 适用于 Executor, HybridExecutor 或任何兼容的执行器
};
```

### 3. AnyScheduler - 类型擦除包装

```cpp
class AnyScheduler {
    // 类型擦除的调度器包装
    // 提供运行时多态，支持存储不同类型的调度器
};
```

## 使用方式

### 方式 1: 直接使用模板调度器

```cpp
// 使用 Executor
EventLoop io_loop;
Executor executor(4);
Scheduler<Executor> scheduler(io_loop, executor);

// 使用 HybridExecutor
HybridExecutor hybrid(io_loop, 4);
Scheduler<HybridExecutor> scheduler(io_loop, hybrid);
```

### 方式 2: 使用类型别名

```cpp
// CPUScheduler 是 Scheduler<Executor> 的别名
CPUScheduler cpu_scheduler(io_loop, executor);

// HybridScheduler 是 Scheduler<HybridExecutor> 的别名
HybridScheduler hybrid_scheduler(io_loop, hybrid);
```

### 方式 3: 使用 IScheduler 接口

```cpp
void process_tasks(IScheduler& scheduler) {
    scheduler.schedule(task, [](const MoltResult& r) {
        // 处理结果
    });
}

// 可以传入任何 Scheduler<T>
CPUScheduler cpu_sched(io_loop, executor);
process_tasks(cpu_sched);

HybridScheduler hybrid_sched(io_loop, hybrid);
process_tasks(hybrid_sched);  // 相同的代码！
```

### 方式 4: 使用 AnyScheduler 实现运行时切换

```cpp
// 存储不同类型的调度器
std::vector<AnyScheduler> schedulers;

schedulers.emplace_back(io_loop1, executor1);      // Executor
schedulers.emplace_back(io_loop2, hybrid_executor);  // HybridExecutor

// 统一使用
for (auto& sched : schedulers) {
    sched.schedule(task, callback);
}
```

## 设计对比

| 特性 | Scheduler<T> | AnyScheduler |
|------|--------------|--------------|
| **类型安全** | 编译期检查 | 运行期检查 |
| **性能** | 零开销（内联） | 虚函数调用 |
| **存储** | 需要具体类型 | 可异构存储 |
| **切换** | 编译期 | 运行期 |
| **大小** | 取决于 T | 一个指针 |
| **使用场景** | 性能关键、类型固定 | 需要运行时多态 |

## 接口层次

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Scheduler Interface Hierarchy                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐  │
│  │                        IScheduler (Abstract)                          │  │
│  │  ┌──────────────────────────────────────────────────────────────┐   │  │
│  │  │  - schedule()                                                  │   │  │
│  │  │  - schedule_delayed()                                         │   │  │
│  │  │  - schedule_with_timeout()                                    │   │  │
│  │  │  - cancel()                                                    │   │  │
│  │  │  - get_status(), get_stats()                                  │   │  │
│  │  │  - shutdown(), is_running()                                   │   │  │
│  │  └──────────────────────────────────────────────────────────────┘   │  │
│  └─────────────────────────────────────────────────────────────────────┘  │
│                                    ▲                                       │
│                                    │ implements                             │
│        ┌───────────────────────────┴─────────────────────────────┐         │
│        │                                                         │         │
│  ┌─────┴──────────┐                                    ┌────────┴─────────┐    │
│  │ Scheduler<T>   │                                    │  AnyScheduler    │    │
│  │ (Template)     │                                    │  (Type-erased)   │    │
│  ├────────────────┤                                    ├─────────────────┤    │
│  │ • Zero overhead│                                    │ • Runtime poly  │    │
│  │ • Type safe    │                                    │ • Heterogeneous │    │
│  │ • Compile-time │                                    │ • Virtual calls │    │
│  └────────────────┘                                    └─────────────────┘    │
│                                                                              │
│  Usage:                                                                      │
│  ┌─────────────────────────────────────────────────────────────────────┐  │
│  │  // Template - compile time selection                                 │  │
│  │  Scheduler<Executor> cpu_sched(io_loop, executor);                    │  │
│  │                                                                         │  │
│  │  // Interface - compile time polymorphism                              │  │
│  │  void func(IScheduler& sched) { /* ... */ }                            │  │
│  │  func(cpu_sched);  // Works!                                           │  │
│  │                                                                         │  │
│  │  // Type-erased - runtime polymorphism                                 │  │
│  │  AnyScheduler any_sched = std::move(cpu_sched);                        │  │
│  │  std::vector<AnyScheduler> schedulers;                                 │  │
│  └─────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 类型别名

```cpp
using CPUScheduler = Scheduler<Executor>;           // CPU 线程池调度器
using HybridScheduler = Scheduler<HybridExecutor>;   // 混合调度器
```

## 工厂模式示例

```cpp
enum class SchedulerType { CPU_ONLY, HYBRID };

auto create_scheduler(SchedulerType type, EventLoop& loop, size_t threads)
    -> AnyScheduler {
    switch (type) {
        case SchedulerType::CPU_ONLY:
            return AnyScheduler(loop, *new Executor(threads));
        case SchedulerType::HYBRID:
            return AnyScheduler(loop, *new HybridExecutor(loop, threads));
    }
}

// 使用配置创建
auto config = read_config();  // 从配置文件读取
auto scheduler = create_scheduler(config.scheduler_type, io_loop, config.num_threads);
```

## 依赖注入示例

```cpp
class AgentService {
    AnyScheduler scheduler_;

public:
    AgentService(AnyScheduler scheduler)
        : scheduler_(std::move(scheduler)) {}

    auto execute_task(const MoltTask& task) -> void {
        scheduler_.schedule(task, [this](const MoltResult& result) {
            handle_result(result);
        });
    }
};

// 测试时注入模拟调度器
class MockScheduler : public IScheduler {
    // 模拟实现
};

// 生产环境注入真实调度器
EventLoop io_loop;
Executor executor(4);
Scheduler<Executor> real_scheduler(io_loop, executor);
AgentService service(AnyScheduler(std::move(real_scheduler)));
```

## 概念约束

```cpp
template <typename S>
concept SchedulerLike = requires(S& s, const MoltTask& t, IScheduler::Callback&& cb) {
    { s.schedule(t, std::move(cb)) } -> std::same_as<bool>;
    { s.get_stats() } -> std::convertible_to<IScheduler::Stats>;
};

template <SchedulerLike S>
auto process_tasks(S& scheduler) -> void {
    // 编译期保证 S 有正确的接口
}
```

## Future API

```cpp
// 与任何调度器类型兼容
auto future = schedule_future(scheduler, task);
auto result = future.get();  // 阻塞等待

// 带超时
auto future = schedule_future_with_timeout(scheduler, task, 5000);
auto result = future.get();
```

## 协程支持

```cpp
// 使用协程等待任务完成
auto process_task(Scheduler<Executor>& scheduler, MoltTask task)
    -> Task<void> {
    auto result = co_await schedule_async(scheduler, task);
    if (result.success) {
        handle_success(result);
    }
}

// AnyScheduler 也支持
auto process_task(AnyScheduler& scheduler, MoltTask task)
    -> Task<void> {
    auto result = co_await schedule_async(scheduler, task);
    // ...
}
```

## 迁移指南

### 从旧的单一类型调度器迁移

**旧代码：**
```cpp
Scheduler scheduler(io_loop, executor);  // 固定使用 Executor
```

**新代码（保持相同行为）：**
```cpp
Scheduler<Executor> scheduler(io_loop, executor);
// 或使用别名
CPUScheduler scheduler(io_loop, executor);
```

**新代码（支持混合路由）：**
```cpp
HybridScheduler scheduler(io_loop, hybrid_executor);
```

**新代码（支持运行时切换）：**
```cpp
AnyScheduler scheduler(io_loop, executor);
// 或
AnyScheduler scheduler(io_loop, hybrid_executor);
```

## 总结

统一调度器接口的优势：

1. **零成本抽象**：模板版本无运行时开销
2. **类型安全**：编译期检查接口正确性
3. **运行时多态**：AnyScheduler 支持异构存储
4. **依赖注入**：IScheduler 接口便于测试
5. **灵活切换**：上层代码无需修改即可切换实现
6. **C++20 兼容**：支持 concepts 和协程
