<div align="center">

# 🐱MoltCat

Agent Collaboration Management Software - A high-performance multi-agent collaboration platform built with C++20.

</div>

## Features

- **Plugin Architecture**: Extend functionality through clean interfaces
- **High Performance**: Powered by libuv event loop and C++20 coroutines
- **Real-time Communication**: HTTP and WebSocket protocol support
- **TUI Interface**: Terminal user interface built with Facebook Yoga
- **Cross-platform**: Windows, Linux, macOS support

## Technology Stack

| Component | Technology |
|-----------|-----------|
| Language | C++20 |
| Event Loop | libuv |
| HTTP | cpp-httplib |
| WebSocket | uWebSockets |
| JSON | Glaze |
| UI Layout | Facebook Yoga |
| Package Manager | vcpkg |
| Build System | CMake |

## Quick Start

### Prerequisites

- C++20 compiler (GCC 11+, Clang 13+, MSVC 2022+)
- CMake 3.20+
- vcpkg
- Git

### Build Steps

```bash
# 1. Clone repository
git clone https://github.com/your/moltcat.git
cd moltcat

# 2. Install dependencies
vcpkg install --triplet=x64-windows

# 3. Build
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

## Directory Structure

```
moltcat/
├── include/moltcat/       # Plugin interfaces
│   ├── molt_model.hpp     # Data models
│   └── molt_skill.hpp     # Skill interfaces
├── source/                # Source code
│   ├── agent/            # Agent management
│   ├── model/            # Data models
│   ├── skill/            # Skill system
│   ├── gateway/          # Gateway layer
│   ├── router/           # Router layer
│   ├── worker/           # Worker layer
│   ├── tui/              # Terminal UI
│   ├── core/             # Core facilities
│   ├── network/          # Network layer
│   ├── storage/          # Storage layer
│   └── utils/            # Utilities
├── examples/             # Example code
├── tests/                # Test code
└── docs/                 # Documentation
```

## Plugin Development

Creating a custom skill takes just three steps:

1. Inherit from `IMoltSkill` interface
2. Implement virtual functions
3. Use `MOLT_EXPORT_SKILL` macro to export

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for details.

## Documentation

- [Architecture Design](docs/ARCHITECTURE.md)
- [Plugin Development Guide](docs/plugin-guide.md) (TBD)

## License

MIT License
