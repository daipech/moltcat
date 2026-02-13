cmake_minimum_required(VERSION 3.20)
project(moltcat
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "MoltCat Multi-Agent Management"
)

# ================================
# C++20 配置
# ================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 导出编译命令
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ================================
# Build options
# ================================
option(MOLTCAT_BUILD_TESTS "Build tests" ON)
option(MOLTCAT_BUILD_EXAMPLES "Build examples" ON)
option(MOLTCAT_ENABLE_TUI "Enable TUI interface" ON)
option(MOLTCAT_ENABLE_SANITIZERS "Enable address/undefined behavior sanitizers" OFF)

# ================================
# Dependency packages
# ================================
find_package(libuv CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(uWebSockets CONFIG REQUIRED)
find_package(glaze CONFIG REQUIRED)
find_package(yoga CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# ================================
# Source file collection
# ================================
# Public interface headers (for plugin developers)
set(MOLTCAT_INTERFACE_HEADERS
    include/moltcat/molt_plugin.hpp
    include/moltcat/molt_model.hpp
    include/moltcat/molt_skill.hpp
    include/moltcat/molt_export.hpp
)

# 内部源文件（按模块分组）
set(MOLTCAT_AGENT_SOURCES)
file(GLOB MOLTCAT_AGENT_SOURCES
    "source/agent/*.cpp"
    "source/agent/*.hpp"
)

set(MOLTCAT_MODEL_SOURCES)
file(GLOB MOLTCAT_MODEL_SOURCES
    "source/model/*.cpp"
    "source/model/*.hpp"
)

set(MOLTCAT_SKILL_SOURCES)
file(GLOB MOLTCAT_SKILL_SOURCES
    "source/skill/*.cpp"
    "source/skill/*.hpp"
)

set(MOLTCAT_GATEWAY_SOURCES)
file(GLOB MOLTCAT_GATEWAY_SOURCES
    "source/gateway/*.cpp"
    "source/gateway/*.hpp"
)

set(MOLTCAT_ROUTER_SOURCES)
file(GLOB MOLTCAT_ROUTER_SOURCES
    "source/router/*.cpp"
    "source/router/*.hpp"
)

set(MOLTCAT_WORKER_SOURCES)
file(GLOB MOLTCAT_WORKER_SOURCES
    "source/worker/*.cpp"
    "source/worker/*.hpp"
)

set(MOLTCAT_TUI_SOURCES)
file(GLOB MOLTCAT_TUI_SOURCES
    "source/tui/*.cpp"
    "source/tui/*.hpp"
)

set(MOLTCAT_CORE_SOURCES)
file(GLOB MOLTCAT_CORE_SOURCES
    "source/core/*.cpp"
    "source/core/*.hpp"
)

set(MOLTCAT_NETWORK_SOURCES)
file(GLOB MOLTCAT_NETWORK_SOURCES
    "source/network/*.cpp"
    "source/network/*.hpp"
)

set(MOLTCAT_STORAGE_SOURCES)
file(GLOB MOLTCAT_STORAGE_SOURCES
    "source/storage/*.cpp"
    "source/storage/*.hpp"
)

set(MOLTCAT_UTILS_SOURCES)
file(GLOB MOLTCAT_UTILS_SOURCES
    "source/utils/*.cpp"
    "source/utils/*.hpp"
)

# Merge all source files
set(MOLTCAT_ALL_SOURCES
    ${MOLTCAT_INTERFACE_HEADERS}
    ${MOLTCAT_AGENT_SOURCES}
    ${MOLTCAT_MODEL_SOURCES}
    ${MOLTCAT_SKILL_SOURCES}
    ${MOLTCAT_GATEWAY_SOURCES}
    ${MOLTCAT_ROUTER_SOURCES}
    ${MOLTCAT_WORKER_SOURCES}
    ${MOLTCAT_CORE_SOURCES}
    ${MOLTCAT_NETWORK_SOURCES}
    ${MOLTCAT_STORAGE_SOURCES}
    ${MOLTCAT_UTILS_SOURCES}
)

# Add TUI source files based on option
if(MOLTCAT_ENABLE_TUI)
    list(APPEND MOLTCAT_ALL_SOURCES ${MOLTCAT_TUI_SOURCES})
endif()

# ================================
# Core library
# ================================
add_library(moltcat_core ${MOLTCAT_ALL_SOURCES})

target_include_directories(moltcat_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

# Link dependency libraries
target_link_libraries(moltcat_core
    PUBLIC
        uv::uv_a
        httplib::httplib
        uWS::uWS
        glaze::glaze
        yoga::yoga
        fmt::fmt
        spdlog::spdlog
)

# C++20 standard feature
target_compile_features(moltcat_core PUBLIC cxx_std_20)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(moltcat_core PRIVATE
        _WIN32_WINNT=0x0A00
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# ================================
# Compiler options
# ================================
if(MSVC)
    target_compile_options(moltcat_core PRIVATE
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
        /Zc:externC-  # 严格的extern "C"规则
    )
    # Enable sanitizers
    if(MOLTCAT_ENABLE_SANITIZERS)
        message(WARNING "MSVC does not support AddressSanitizer, use clang-cl or GCC")
    endif()
else()
    target_compile_options(moltcat_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wcast-align
        -Wdouble-promotion
    )
    # GCC/Clang sanitizers
    if(MOLTCAT_ENABLE_SANITIZERS)
        target_compile_options(moltcat_core PUBLIC
            -fsanitize=address,undefined
            -fno-sanitize-recover=address,undefined
        )
        target_link_options(moltcat_core PUBLIC
            -fsanitize=address,undefined
        )
    endif()
endif()

# ================================
# Executable
# ================================
add_executable(moltcat source/main.cpp)
target_link_libraries(moltcat PRIVATE moltcat_core)

# ================================
# Tests
# ================================
if(MOLTCAT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ================================
# Examples
# ================================
if(MOLTCAT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ================================
# Installation configuration
# ================================
include(GNUInstallDirs)

install(TARGETS moltcat moltcat_core
    EXPORT moltcat_targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${MOLTCAT_INTERFACE_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/moltcat
)

install(EXPORT moltcat_targets
    FILE moltcat_targets.cmake
    NAMESPACE moltcat::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/moltcat
)

# ================================
# Configuration package files
# ================================
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/moltcat-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/moltcat-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/moltcat-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/moltcat
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/moltcat-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/moltcat-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/moltcat
)
