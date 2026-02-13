# MoltCat 架构设计文档

## 1. 项目概述

**MoltCat** 是一个智能体协作管理软件，旨在提供高效、可扩展的多智能体协同工作平台。

### 1.1 核心目标

- 提供统一的多智能体管理和编排能力
- 支持智能体间的高效通信与协作
- 提供直观的终端用户界面(TUI)
- 确保高性能和低资源占用

## 2. 技术选型

| 组件 | 技术选择 | 版本要求 | 用途说明 |
|------|---------|---------|---------|
| 编程语言 | C++20 | GCC 11+/Clang 13+/MSVC 2022+ | 现代C++特性支持 |
| 事件循环 | libuv | 1.x | 跨平台异步I/O |
| HTTP服务器 | cpp-httplib | 0.15+ | RESTful API服务 |
| WebSocket | uWebSockets | 20.x | 实时双向通信 |
| JSON解析 | glaze | 2.x+ | 高性能JSON序列化 |
| UI布局 | Facebook Yoga | 2.x+ | Flexbox布局引擎 |
| 包管理 | vcpkg | 2024+ | C++依赖管理 |
| 构建系统 | CMake | 3.20+ | 跨平台构建 |

## 3. 技术选型理由

### 3.1 C++20

- **协程支持**: 原生协程简化异步代码编写
- **Concepts**: 更强的类型约束和编译期检查
- **Modules**: 逐步采用模块化编译（编译器支持成熟后）
- **Ranges**: 函数式编程风格的数据处理

### 3.2 libuv

- **跨平台**: 统一Windows/Linux/macOS异步I/O接口
- **高性能**: 事件驱动架构，低延迟
- **成熟稳定**: Node.js底层核心，久经考验

### 3.3 cpp-httplib

- **Header-only**: 轻量级集成
- **简洁API**: 易于实现RESTful接口
- **HTTP/HTTPS**: 支持加密传输

### 3.4 uWebSockets

- **极致性能**: 业界最快的WebSocket实现之一
- **低内存占用**: 零拷贝设计
- **可扩展**: 支持百万级并发连接

### 3.5 Glaze

- **编译期反射**: 无需手动编写序列化代码
- **高性能**: 比RapidJSON/nlohmannjson更快
- **Header-only**: 便捷集成

### 3.6 Facebook Yoga

- **Flexbox标准**: Web开发者熟悉的布局模型
- **跨平台TUI**: 灵活的终端界面布局
- **高效计算**: 增量布局更新

## 4. 系统架构

### 4.1 分层架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         MoltCat 插件化架构                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    插件扩展层 (include/)                         │    │
│  │   ┌────────────────────┐        ┌─────────────────────┐         │    │
│  │   │  molt_model.hpp    │        │  molt_skill.hpp     │         │    │
│  │   │  [数据模型接口]    │        │  [技能扩展接口]     │         │    │
│  │   └────────────────────┘        └─────────────────────┘         │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                ↕ 实现                                   │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                      表现层 (source/tui)                         │    │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────────────┐  │    │
│  │  │ 仪表板  │ │ 智能体  │ │ 日志    │ │ 监控面板                │  │    │
│  │  │ 视图    │ │ 列表    │ │ 视图    │ │                         │  │    │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────────────┘  │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                ↕                                        │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                      网关层 (source/gateway)                      │    │
│  │  ┌──────────────────────┐           ┌────────────────────┐       │    │
│  │  │   HTTP Gateway       │           │  WebSocket Gateway │       │    │
│  │  │   (RESTful API)      │           │  (实时通信)        │       │    │
│  │  └──────────────────────┘           └────────────────────┘       │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                ↕                                        │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                      路由层 (source/router)                       │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐     │    │
│  │  │ Task Router │ │Event Router │ │    Load Balancer       │     │    │
│  │  │  任务分发   │ │  事件分发   │ │    负载均衡            │     │    │
│  │  └─────────────┘ └─────────────┘ └─────────────────────────┘     │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                ↕                                        │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                   工作层 (source/worker)                         │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐     │    │
│  │  │ Worker Pool │ │ Task Queue  │ │     Scheduler          │     │    │
│  │  │  工作池     │ │  任务队列   │ │     调度器             │     │    │
│  │  └─────────────┘ └─────────────┘ └─────────────────────────┘     │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                ↕                                        │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    业务逻辑层 (source/)                          │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐     │    │
│  │  │   Agent     │ │   Skill     │ │      Model             │     │    │
│  │  │  智能体管理  │ │  技能系统   │ │      数据模型          │     │    │
│  │  │  /agent/    │ │  /skill/    │ │      /model/           │     │    │
│  │  └─────────────┘ └─────────────┘ └─────────────────────────┘     │    │
│  │  ┌─────────────────────────────────────────────────────────┐    │    │
│  │  │              Event Bus / Orchestrator                   │    │    │
│  │  │                   事件总线 / 编排器                      │    │    │
│  │  └─────────────────────────────────────────────────────────┘    │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                ↕                                        │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    网络层 (source/network)                       │    │
│  │  ┌──────────────────────┐           ┌────────────────────┐       │    │
│  │  │   HTTP Server        │           │  WebSocket Server  │       │    │
│  │  │   (cpp-httplib)      │           │  (uWebSockets)     │       │    │
│  │  └──────────────────────┘           └────────────────────┘       │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                ↕                                        │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    I/O 层 (libuv)                                │    │
│  │         事件循环 │ 定时器 │ 文件系统 │ 网络I/O                    │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                ↕                                        │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                  存储层 (source/storage)                         │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────────────┐  │    │
│  │  │ 内存缓存 │ │ 本地存储 │ │ 配置文件 │ │    日志系统         │  │    │
│  │  └──────────┘ └──────────┘ └──────────┘ └────────────────────┘  │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                           │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.2 数据流

```
外部请求 → Gateway → Router → Worker Pool → Agent/Skill → Model → Storage
    ↑                                                        ↓
    └────────────────────────────────────────────────────────┘
                         TUI 实时更新
```

## 5. 目录结构

### 5.1 插件化架构设计

MoltCat 采用**插件化架构**：`include/` 仅暴露核心扩展接口，所有具体实现均在 `source/` 内部。

```
moltcat/
├── CMakeLists.txt              # CMake 构建配置
├── vcpkg.json                  # vcpkg 依赖声明
├── .clang-format               # 代码格式化配置
├── .clang-tidy                 # 静态分析配置
├── include/
│   └── moltcat/
│       ├── molt_model.hpp      # [插件接口] 数据模型抽象
│       └── molt_skill.hpp      # [插件接口] 技能扩展接口
├── source/
│   ├── agent/                  # 智能体核心实现
│   │   ├── agent_manager.hpp/cpp
│   │   ├── agent_registry.hpp/cpp
│   │   └── agent_lifecycle.hpp/cpp
│   ├── model/                  # 数据模型实现
│   │   ├── task.hpp/cpp        # 任务定义
│   │   ├── context.hpp/cpp     # 执行上下文
│   │   ├── result.hpp/cpp      # 执行结果
│   │   └── state.hpp/cpp       # 状态管理
│   ├── skill/                  # 技能系统实现
│   │   ├── skill_loader.hpp/cpp
│   │   ├── skill_executor.hpp/cpp
│   │   └── skill_registry.hpp/cpp
│   ├── gateway/                # 网关层（对外通信）
│   │   ├── http_gateway.hpp/cpp
│   │   ├── ws_gateway.hpp/cpp
│   │   └── protocol.hpp/cpp
│   ├── router/                 # 路由分发层
│   │   ├── task_router.hpp/cpp
│   │   ├── event_router.hpp/cpp
│   │   └── load_balancer.hpp/cpp
│   ├── worker/                 # 工作线程/协程池
│   │   ├── worker_pool.hpp/cpp
│   │   ├── task_queue.hpp/cpp
│   │   └── scheduler.hpp/cpp
│   ├── tui/                    # 终端用户界面
│   │   ├── terminal.hpp/cpp
│   │   ├── layout.hpp/cpp
│   │   ├── widgets.hpp/cpp
│   │   └── renderer.hpp/cpp
│   ├── core/                   # 核心基础设施
│   │   ├── event_bus.hpp/cpp
│   │   ├── orchestrator.hpp/cpp
│   │   └── config.hpp/cpp
│   ├── network/                # 网络层
│   │   ├── http_server.hpp/cpp
│   │   ├── ws_server.hpp/cpp
│   │   └── tls.hpp/cpp
│   ├── storage/                # 持久化层
│   │   ├── cache.hpp/cpp
│   │   ├── database.hpp/cpp
│   │   └── log_store.hpp/cpp
│   ├── utils/                  # 工具类
│   │   ├── json.hpp/cpp
│   │   ├── logger.hpp/cpp
│   │   └── uuid.hpp/cpp
│   └── main.cpp                # 程序入口
├── tests/                      # 测试代码
├── docs/                       # 文档
└── examples/                   # 示例代码
```

### 5.2 目录职责说明

| 目录 | 职责 | 说明 |
|------|------|------|
| `include/moltcat/` | **插件接口层** | 仅包含扩展所需的接口定义，外部插件只需包含此目录 |
| `source/agent/` | 智能体管理 | 智能体生命周期管理、注册、调度 |
| `source/model/` | 数据模型 | 任务、上下文、结果、状态等核心数据结构 |
| `source/skill/` | 技能系统 | 技能加载、执行、注册机制 |
| `source/gateway/` | 网关层 | 对外通信入口（HTTP/WebSocket） |
| `source/router/` | 路由层 | 任务分发、事件路由、负载均衡 |
| `source/worker/` | 工作层 | 任务执行池、调度器 |
| `source/tui/` | 用户界面 | 终端界面渲染与交互 |
| `source/core/` | 核心设施 | 事件总线、编排器、配置 |
| `source/network/` | 网络层 | 底层网络通信实现 |
| `source/storage/` | 存储层 | 缓存、数据库、日志存储 |
| `source/utils/` | 工具类 | JSON、日志、UUID等通用工具 |

## 6. 核心模块设计

### 6.1 插件接口层 (include/)

#### molt_model.hpp - 数据模型接口

```cpp
#pragma once
#include <string>
#include <memory>
#include <glaze/json.hpp>

namespace moltcat {

// 任务数据结构（供插件实现/扩展）
struct MoltTask {
    std::string id;
    std::string type;
    glz::json_t payload;
    int priority = 0;
    uint64_t timeout_ms = 30000;
};

// 执行结果（供插件实现/扩展）
struct MoltResult {
    bool success = false;
    glz::json_t data;
    std::string error_message;
    uint64_t execution_time_ms = 0;
};

// 执行上下文（供插件使用）
struct MoltContext {
    std::string agent_id;
    std::string task_id;
    glz::json_t shared_state;
    glz::json_t user_data;
};

} // namespace moltcat
```

#### molt_skill.hpp - 技能扩展接口

```cpp
#pragma once
#include "molt_model.hpp"
#include <string>
#include <memory>

namespace moltcat {

// 技能接口：外部插件需实现此接口以扩展功能
class IMoltSkill {
public:
    virtual ~IMoltSkill() = default;

    // 技能元信息
    virtual auto name() const -> std::string = 0;
    virtual auto version() const -> std::string = 0;
    virtual auto description() const -> std::string = 0;

    // 能力声明
    virtual auto can_handle(const std::string& task_type) const -> bool = 0;

    // 执行任务
    virtual auto execute(const MoltTask& task, MoltContext& ctx) -> MoltResult = 0;

    // 生命周期回调
    virtual auto initialize() -> bool { return true; }
    virtual void shutdown() {}
};

// 技能工厂：用于动态加载技能插件
class IMoltSkillFactory {
public:
    virtual ~IMoltSkillFactory() = default;
    virtual auto create() -> std::unique_ptr<IMoltSkill> = 0;
    virtual auto type_name() const -> std::string = 0;
};

} // namespace moltcat

// 插件导出宏
#define MOLT_EXPORT_SKILL(FactoryClass) \
    extern "C" { \
        moltcat::IMoltSkillFactory* molt_create_skill_factory() { \
            return new FactoryClass(); \
        } \
        void molt_destroy_skill_factory(moltcat::IMoltSkillFactory* factory) { \
            delete factory; \
        } \
    }
```

### 6.2 智能体层 (source/agent/)

```cpp
// source/agent/agent_manager.hpp
namespace moltcat::agent {

class AgentManager {
public:
    struct Config {
        size_t max_agents = 100;
        size_t worker_threads = 4;
    };

    explicit AgentManager(const Config& config);
    ~AgentManager();

    // 智能体生命周期管理
    auto register_agent(std::string_view id, std::shared_ptr<IMoltSkill> skill) -> bool;
    auto unregister_agent(std::string_view id) -> bool;
    auto get_agent(std::string_view id) -> std::shared_ptr<Agent>;

    // 任务调度
    auto dispatch_task(const MoltTask& task, MoltContext& ctx) -> std::future<MoltResult>;
    auto list_agents() const -> std::vector<std::string>;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace moltcat::agent
```

### 6.3 路由层 (source/router/)

```cpp
// source/router/task_router.hpp
namespace moltcat::router {

class TaskRouter {
public:
    // 路由策略
    enum class Strategy {
        ROUND_ROBIN,    // 轮询
        LEAST_LOADED,   // 最少负载
        PRIORITY,       // 优先级
        AFFINITY,       // 亲和性
    };

    explicit TaskRouter(Strategy strategy = Strategy::LEAST_LOADED);

    // 路由决策
    auto route(const MoltTask& task) -> std::string;  // 返回目标 agent_id

    // 负载报告
    void report_load(std::string_view agent_id, double load_factor);
    void report_completion(std::string_view agent_id, uint64_t exec_time_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace moltcat::router
```

### 6.4 网关层 (source/gateway/)

```cpp
// source/gateway/http_gateway.hpp
namespace moltcat::gateway {

class HttpGateway {
public:
    struct Config {
        std::string host = "0.0.0.0";
        uint16_t port = 8080;
        bool enable_cors = true;
    };

    explicit HttpGateway(const Config& config);
    ~HttpGateway();

    auto start() -> std::future<bool>;
    auto stop() -> std::future<void>;

    // 注册路由处理器
    using RequestHandler = std::function<void(const httplib::Request&, httplib::Response&)>;
    void register_handler(std::string_view pattern, RequestHandler handler);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// source/gateway/ws_gateway.hpp
class WsGateway {
public:
    struct Config {
        std::string host = "0.0.0.0";
        uint16_t port = 9000;
        size_t max_connections = 1000;
    };

    explicit WsGateway(const Config& config);
    ~WsGateway();

    auto start() -> std::future<bool>;
    auto stop() -> std::future<void>;

    using MessageHandler = std::function<void(std::string_view, std::string_view)>;
    void on_message(MessageHandler handler);  // (conn_id, message)

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace moltcat::gateway
```

### 6.5 工作层 (source/worker/)

```cpp
// source/worker/worker_pool.hpp
namespace moltcat::worker {

class WorkerPool {
public:
    struct Config {
        size_t min_workers = 2;
        size_t max_workers = 16;
        size_t queue_size = 10000;
    };

    explicit WorkerPool(const Config& config);
    ~WorkerPool();

    // 提交任务
    using Job = std::function<void()>;
    auto submit(Job&& job) -> std::future<void>;

    // 统计信息
    auto active_worker_count() const -> size_t;
    auto pending_task_count() const -> size_t;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace moltcat::worker
```

### 6.6 TUI层 (source/tui/)

```cpp
// source/tui/terminal.hpp
namespace moltcat::tui {

class TerminalUI {
public:
    struct Config {
        bool enable_colors = true;
        std::string theme = "default";
        int refresh_rate_ms = 16;  // ~60 FPS
    };

    explicit TerminalUI(const Config& config);
    ~TerminalUI();

    auto initialize() -> bool;
    void run();
    void shutdown();

    // 组件管理
    auto add_panel(std::unique_ptr<Panel> panel) -> PanelId;
    void remove_panel(PanelId id);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// source/tui/layout.hpp - Yoga Flexbox 布局封装
class FlexLayout {
public:
    explicit FlexLayout();
    ~FlexLayout();

    // Flexbox 属性设置
    void set_direction(YGDirection direction);
    void set_justify_content(YGJustify justify);
    void set_align_items(YGAlign align);
    void set_flex_grow(float grow);

    // 布局计算
    void calculate_layout(float width, float height);
    auto get_bounds() const -> Rect;

private:
    YGNodeRef node_;
};

} // namespace moltcat::tui
```

## 7. 构建配置

### 7.1 vcpkg.json

```json
{
  "name": "moltcat",
  "version": "0.1.0",
  "description": "智能体协作管理软件",
  "dependencies": [
    {
      "name": "libuv",
      "version>=": "1.46.0"
    },
    {
      "name": "cpp-httplib",
      "version>=": "0.15.0"
    },
    {
      "name": "uwebsockets",
      "version>=": "20.0.0"
    },
    {
      "name": "glaze",
      "version>=": "2.0.0"
    },
    {
      "name": "yoga",
      "version>=": "2.0.0"
    },
    {
      "name": "fmt",
      "version>=": "10.0.0"
    },
    {
      "name": "spdlog",
      "version>=": "1.12.0"
    }
  ],
  "builtin-baseline": "2024-01-01"
}
```

### 7.2 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(moltcat
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "智能体协作管理软件"
)

# C++20 要求
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg 集成
find_package(libuv CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(uWebSockets CONFIG REQUIRED)
find_package(glaze CONFIG REQUIRED)
find_package(yoga CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# 源文件收集
file(GLOB_RECURSE MOLTCAT_SOURCES
    "source/*.cpp"
)

file(GLOB_RECURSE MOLTCAT_HEADERS
    "include/*.hpp"
    "include/*.h"
)

# 主库
add_library(moltcat_lib ${MOLTCAT_SOURCES} ${MOLTCAT_HEADERS})
target_include_directories(moltcat_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_lib
    PUBLIC
        uv::uv_a
        httplib::httplib
        uWS::uWS
        glaze::glaze
        yoga::yoga
        fmt::fmt
        spdlog::spdlog
)

# 可执行文件
add_executable(moltcat source/main.cpp)
target_link_libraries(moltcat PRIVATE moltcat_lib)

# 编译选项
if(MSVC)
    target_compile_options(moltcat_lib PRIVATE
        /W4 /WX /permissive- /Zc:__cplusplus
    )
else()
    target_compile_options(moltcat_lib PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

# 测试
option(BUILD_TESTING "构建测试" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
```

## 8. API设计

### 8.1 RESTful API

| 端点 | 方法 | 描述 |
|------|------|------|
| `/api/v1/agents` | GET | 列出所有智能体 |
| `/api/v1/agents` | POST | 注册新智能体 |
| `/api/v1/agents/{id}` | GET | 获取智能体详情 |
| `/api/v1/agents/{id}` | DELETE | 注销智能体 |
| `/api/v1/agents/{id}/tasks` | POST | 向智能体提交任务 |
| `/api/v1/agents/{id}/status` | GET | 获取智能体状态 |
| `/api/v1/tasks` | GET | 列出所有任务 |
| `/api/v1/tasks/{id}` | GET | 获取任务详情 |
| `/api/v1/tasks/{id}/result` | GET | 获取任务结果 |

### 8.2 WebSocket协议

```javascript
// 客户端 → 服务器
{
  "type": "register_agent",
  "data": {
    "name": "agent-name",
    "capabilities": ["task-type-1", "task-type-2"]
  }
}

// 服务器 → 客户端
{
  "type": "task_assigned",
  "data": {
    "task_id": "uuid",
    "payload": { ... }
  }
}
```

## 9. 编码规范

### 9.1 命名约定

| 类型 | 约定 | 示例 |
|------|------|------|
| 类名 | `PascalCase` | `AgentManager`, `TaskRouter` |
| 函数名 | `snake_case` | `register_agent()`, `dispatch_task()` |
| 成员变量 | `trailing_underscore_` | `max_workers_`, `active_agents_` |
| 常量 | `UPPER_SNAKE_CASE` | `MAX_AGENTS`, `DEFAULT_TIMEOUT` |
| 接口类 | `I` 前缀 | `IMoltSkill`, `IMoltSkillFactory` |
| 私有实现 | `Impl` 惯用法 | PIMPL 模式隐藏实现细节 |

### 9.2 文件组织

- **include/**: 仅包含插件扩展接口
  - `include/moltcat/molt_model.hpp`
  - `include/moltcat/molt_skill.hpp`

- **source/**: 所有具体实现
  - 头文件和源文件对应: `source/agent/agent_manager.hpp` + `source/agent/agent_manager.cpp`
  - 内部头文件使用相对路径包含

- **模块化原则**:
  - 每个 `.hpp` 对应一个 `.cpp`
  - 保持接口最小化，实现细节隐藏在 `.cpp` 中

### 9.3 插件开发规范

外部插件开发者只需：
1. 包含 `#include <moltcat/molt_skill.hpp>`
2. 继承 `moltcat::IMoltSkill` 接口
3. 实现 `moltcat::IMoltSkillFactory` 工厂类
4. 使用 `MOLT_EXPORT_SKILL` 宏导出

### 9.4 注释语言

- 所有注释使用**简体中文**
- Doxygen 风格文档注释
- 公共API必须包含完整文档

## 10. 性能目标

| 指标 | 目标值 |
|------|--------|
| 启动时间 | < 500ms |
| 内存占用 | < 100MB (空闲) |
| 单个智能体调度延迟 | < 10ms |
| WebSocket吞吐量 | > 100k msg/s |
| HTTP请求响应 | < 50ms (p99) |
| TUI刷新率 | 60 FPS |

## 11. 安全考虑

- TLS/SSL 加密通信
- 智能体认证与授权
- 输入验证与清洗
- 资源限制（CPU/内存）
- 审计日志

## 12. 开发路线图

### Phase 1: 基础框架 (Week 1-2)
- [x] 项目结构搭建
- [x] 架构文档编写
- [ ] CMake/vcpkg 构建系统配置
- [ ] 插件接口定义 (`include/moltcat/*.hpp`)
- [ ] 基础工具类实现 (`source/utils/`)

### Phase 2: 核心业务层 (Week 3-5)
- [ ] 数据模型实现 (`source/model/`)
- [ ] 技能系统实现 (`source/skill/`)
- [ ] 智能体管理器 (`source/agent/`)
- [ ] 事件总线 (`source/core/`)
- [ ] 任务编排引擎 (`source/core/`)

### Phase 3: 路由与工作层 (Week 6-7)
- [ ] 任务路由器 (`source/router/`)
- [ ] 工作线程池 (`source/worker/`)
- [ ] 负载均衡器 (`source/router/`)
- [ ] 任务队列 (`source/worker/`)

### Phase 4: 网关与网络层 (Week 8-9)
- [ ] HTTP 网关 (`source/gateway/`)
- [ ] WebSocket 网关 (`source/gateway/`)
- [ ] 协议定义与实现 (`source/gateway/protocol.hpp`)
- [ ] 底层网络层封装 (`source/network/`)

### Phase 5: 存储与TUI (Week 10-11)
- [ ] 缓存系统 (`source/storage/`)
- [ ] 本地存储 (`source/storage/`)
- [ ] TUI 框架搭建 (`source/tui/`)
- [ ] Yoga 布局集成 (`source/tui/layout.hpp`)
- [ ] 交互逻辑实现 (`source/tui/`)

### Phase 6: 插件系统与测试 (Week 12-13)
- [ ] 插件动态加载机制
- [ ] 示例插件开发 (`examples/`)
- [ ] 单元测试 (`tests/`)
- [ ] 集成测试 (`tests/integration/`)
- [ ] 插件开发文档 (`docs/plugin-guide.md`)

### Phase 7: 优化与发布 (Week 14-15)
- [ ] 性能优化与压测
- [ ] 文档完善
- [ ] CI/CD 配置
- [ ] v0.1.0 发布准备

---

**文档版本**: 2.0
**最后更新**: 2026-02-13
**维护者**: MoltCat 开发团队
