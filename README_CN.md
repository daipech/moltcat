# MoltCat

智能体协作管理软件 - 基于 C++20 的高性能多智能体协同平台。

## 特性

- **插件化架构**: 通过简洁的接口扩展功能
- **高性能**: 基于 libuv 事件循环和 C++20 协程
- **实时通信**: 支持 HTTP 和 WebSocket 协议
- **TUI界面**: 基于 Facebook Yoga 的终端用户界面
- **跨平台**: 支持 Windows、Linux、macOS

## 技术栈

| 组件 | 技术 |
|------|------|
| 语言 | C++20 |
| 事件循环 | libuv |
| HTTP | cpp-httplib |
| WebSocket | uWebSockets |
| JSON | Glaze |
| UI布局 | Facebook Yoga |
| 包管理 | vcpkg |
| 构建 | CMake |

## 快速开始

### 前置要求

- C++20 编译器 (GCC 11+, Clang 13+, MSVC 2022+)
- CMake 3.20+
- vcpkg
- Git

### 构建步骤

```bash
# 1. 克隆仓库
git clone https://github.com/your/moltcat.git
cd moltcat

# 2. 安装依赖
vcpkg install --triplet=x64-windows

# 3. 构建
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg根目录]/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

## 目录结构

```
moltcat/
├── include/moltcat/       # 插件接口
│   ├── molt_model.hpp     # 数据模型
│   └── molt_skill.hpp     # 技能接口
├── source/                # 源代码
│   ├── agent/            # 智能体管理
│   ├── model/            # 数据模型
│   ├── skill/            # 技能系统
│   ├── gateway/          # 网关层
│   ├── router/           # 路由层
│   ├── worker/           # 工作层
│   ├── tui/              # 终端界面
│   ├── core/             # 核心设施
│   ├── network/          # 网络层
│   ├── storage/          # 存储层
│   └── utils/            # 工具类
├── examples/             # 示例代码
├── tests/                # 测试代码
└── docs/                 # 文档
```

## 开发插件

创建自定义技能只需三步：

1. 继承 `IMoltSkill` 接口
2. 实现虚函数
3. 使用 `MOLT_EXPORT_SKILL` 宏导出

详见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 文档

- [架构设计](docs/ARCHITECTURE.md)
- [插件开发指南](docs/plugin-guide.md) (待补充)

## 许可证

MIT License
