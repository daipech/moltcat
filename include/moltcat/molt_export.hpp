/**
 * @file molt_export.hpp
 * @brief MoltCat plugin export definitions
 * @note Contains C interface and platform-specific export/import macros
 */

#pragma once

#include "molt_plugin.hpp"
#include "molt_model.hpp"

// ============================================================
// Platform-specific export macros
// ============================================================

// Define MOLTCAT_BUILD when building the DLL/so
// Leave undefined when consuming the DLL/so

#ifdef _WIN32
    // Windows DLL export/import
    #ifdef MOLTCAT_BUILD
        // Building the DLL - export symbols
        #define MOLTCAT_API __declspec(dllexport)
    #else
        // Consuming the DLL - import symbols
        #define MOLTCAT_API __declspec(dllimport)
    #endif
#else
    // POSIX shared library visibility
    #ifdef MOLTCAT_BUILD
        #define MOLTCAT_API __attribute__((visibility("default")))
    #else
        #define MOLTCAT_API
    #endif
#endif

// ============================================================
// C API export functions
// ============================================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create skill factory instance
 * @return Factory instance pointer, framework responsible for destruction
 *
 * @note This function is exported from the plugin DLL/shared library
 *       The framework calls this to create skill instances
 */
MOLTCAT_API auto molt_create_skill_factory() -> moltcat::IMoltSkillFactory*;

/**
 * @brief Destroy skill factory instance
 * @param factory Factory instance pointer to destroy
 *
 * @note Call this to properly clean up the factory and all associated resources
 */
MOLTCAT_API void molt_destroy_skill_factory(moltcat::IMoltSkillFactory* factory);

/**
 * @brief Get plugin API version
 * @return API version string (e.g., "1.0.0")
 *
 * @note Used for compatibility checking between framework and plugin
 */
MOLTCAT_API auto molt_get_plugin_api_version() -> const char*;

/**
 * @brief Get plugin information
 * @return Plugin information structure
 *
 * @note Returns metadata about the plugin including name, version, description
 *       Framework is responsible for freeing the string pointers in the returned structure
 */
MOLTCAT_API auto molt_get_plugin_info() -> moltcat::PluginInfo;

/**
 * @brief Initialize plugin
 * @param config_json JSON format configuration string (can be nullptr)
 * @return Returns true on success
 *
 * @note Called once after the plugin is loaded
 *       Use this for plugin-level initialization (loading resources, etc.)
 */
MOLTCAT_API auto molt_plugin_initialize(const char* config_json) -> bool;

/**
 * @brief Shutdown plugin
 *
 * @note Called before the plugin is unloaded
 *       Use this for plugin-level cleanup (releasing resources, etc.)
 */
MOLTCAT_API void molt_plugin_shutdown();

/**
 * @brief Get last error message
 * @return Error message string, or nullptr if no error
 *
 * @note Returns the last error message from the plugin
 *       The string is valid until the next plugin function call
 */
MOLTCAT_API auto molt_get_last_error() -> const char*;

#ifdef __cplusplus
}
#endif

// ============================================================
// Plugin export macro
// ============================================================

/**
 * @brief Export skill plugin
 *
 * This macro generates all required C API functions for a skill plugin.
 * Use it in your plugin source file to export the skill factory.
 *
 * Usage example:
 * @code
 * // MySkillFactory.h
 * class MySkillFactory : public moltcat::IMoltSkillFactory {
 * public:
 *     auto create() -> moltcat::IMoltSkill* override {
 *         return new MySkill();
 *     }
 *     void destroy(moltcat::IMoltSkill* skill) override {
 *         delete skill;
 *     }
 *     auto type_name() const -> const char* override {
 *         return "MySkill";
 *     }
 *     auto get_metadata() const -> moltcat::SkillMetadata override {
 *         // ... return metadata
 *     }
 *     auto get_api_version() const -> const char* override {
 *         return "1.0.0";
 *     }
 * };
 *
 * // MySkillPlugin.cpp
 * #include <moltcat/molt_export.hpp>
 * #include "MySkillFactory.h"
 *
 * MOLT_EXPORT_SKILL(MySkillFactory, "MySkill", "1.0.0")
 * @endcode
 *
 * @param FactoryClass The factory class name (must inherit from IMoltSkillFactory)
 * @param PluginName Plugin name string
 * @param PluginVersion Plugin version string (semantic versioning)
 */
#define MOLT_EXPORT_SKILL(FactoryClass, PluginName, PluginVersion) \
    extern "C" { \
        MOLTCAT_API auto molt_create_skill_factory() -> moltcat::IMoltSkillFactory* { \
            return new FactoryClass(); \
        } \
        MOLTCAT_API void molt_destroy_skill_factory(moltcat::IMoltSkillFactory* factory) { \
            delete factory; \
        } \
        MOLTCAT_API auto molt_get_plugin_api_version() -> const char* { \
            return "1.0.0"; \
        } \
        MOLTCAT_API auto molt_get_plugin_info() -> moltcat::PluginInfo { \
            moltcat::PluginInfo info; \
            info.name = moltcat::IString::create(PluginName); \
            info.version = moltcat::IString::create(PluginVersion); \
            info.description = moltcat::IString::create(""); \
            info.author = moltcat::IString::create(""); \
            info.license = moltcat::IString::create("MIT"); \
            info.homepage = nullptr; \
            return info; \
        } \
        MOLTCAT_API auto molt_plugin_initialize(const char* config_json) -> bool { \
            (void)config_json; \
            return true; \
        } \
        MOLTCAT_API void molt_plugin_shutdown() { \
            /* Clean up resources */ \
        } \
        MOLTCAT_API auto molt_get_last_error() -> const char* { \
            return nullptr; \
        } \
    }
