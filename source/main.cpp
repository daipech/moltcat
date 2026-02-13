/**
 * @file main.cpp
 * @brief MoltCat program entry point
 */

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include <csignal>
#include <atomic>

// Global exit flag
std::atomic<bool> g_running{true};

// Signal handler
void signal_handler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
    g_running = false;
}

int main(int argc, char* argv[]) {
    // Initialize logging
    auto logger = spdlog::stdout_color_mt("moltcat");
    logger->set_level(spdlog::level::info);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");

    spdlog::info("MoltCat v{}.{}.{} starting...",
        MOLTCAT_VERSION_MAJOR,
        MOLTCAT_VERSION_MINOR,
        MOLTCAT_VERSION_PATCH);

    // Register signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

#ifdef _WIN32
    std::signal(SIGBREAK, signal_handler);
#else
    std::signal(SIGHUP, signal_handler);
#endif

    // TODO: Initialize modules
    // - Load configuration
    // - Initialize plugin system
    // - Start network services
    // - Start TUI (if enabled)

    spdlog::info("MoltCat started, press Ctrl+C to exit");

    // Main event loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // TODO: Cleanup resources
    spdlog::info("MoltCat shutdown complete");

    return 0;
}
