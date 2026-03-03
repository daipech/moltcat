cmake_minimum_required(VERSION 3.20)

# ================================
# vcpkg support
# ================================
# To use vcpkg, set CMAKE_TOOLCHAIN_FILE before project():
# cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake ..
# Or set VCPKG_ROOT environment variable and it will be auto-detected
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(moltcat
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "MoltCat Multi-Agent Management"
)

# ================================
# C++20 Configuration
# ================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ================================
# Build options
# ================================
option(MOLTCAT_BUILD_TESTS "Build tests" ON)
option(MOLTCAT_BUILD_EXAMPLES "Build examples" ON)
option(MOLTCAT_ENABLE_SANITIZERS "Enable address/undefined behavior sanitizers" OFF)

# ================================
# Dependency packages
# ================================
find_package(libuv CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(unofficial-uwebsockets CONFIG REQUIRED)
find_package(glaze CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(ZeroMQ CONFIG REQUIRED)
find_package(cppzmq CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# ================================
# Add module subdirectories (in dependency order)
# ================================
add_subdirectory(source/utils)
add_subdirectory(source/model)
add_subdirectory(source/protocol)
add_subdirectory(source/network)
add_subdirectory(source/monitoring)
add_subdirectory(source/messaging)
add_subdirectory(source/core)
add_subdirectory(source/memory)
add_subdirectory(source/provider)
add_subdirectory(source/agent)
add_subdirectory(source/gateway)

# ================================
# Public interface headers (for plugin developers)
# ================================
set(MOLTCAT_INTERFACE_HEADERS
    include/moltcat/molt_plugin.hpp
    include/moltcat/molt_model.hpp
    include/moltcat/molt_skill.hpp
    include/moltcat/molt_export.hpp
)

# ================================
# Executable
# ================================
add_executable(moltcat source/main.cpp)

# Link all module libraries and dependencies
target_link_libraries(moltcat
    PRIVATE
        moltcat_utils
        moltcat_model
        moltcat_protocol
        moltcat_network
        moltcat_monitoring
        moltcat_messaging
        moltcat_core
        moltcat_memory
        moltcat_provider
        moltcat_agent
        moltcat_gateway
        $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
        httplib::httplib
        unofficial::uwebsockets::uwebsockets
        fmt::fmt
        spdlog::spdlog
)

# C++20 standard feature
target_compile_features(moltcat PUBLIC cxx_std_20)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(moltcat PRIVATE
        _WIN32_WINNT=0x0A00
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# ================================
# Compiler options
# ================================
if(MSVC)
    target_compile_options(moltcat PRIVATE
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
        /Zc:externC-  # Strict extern "C" rules
    )
    # Enable sanitizers
    if(MOLTCAT_ENABLE_SANITIZERS)
        message(WARNING "MSVC does not support AddressSanitizer, use clang-cl or GCC")
    endif()
else()
    target_compile_options(moltcat PRIVATE
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
        target_compile_options(moltcat PUBLIC
            -fsanitize=address,undefined
            -fno-sanitize-recover=address,undefined
        )
        target_link_options(moltcat PUBLIC
            -fsanitize=address,undefined
        )
    endif()
endif()

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

install(TARGETS moltcat
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
