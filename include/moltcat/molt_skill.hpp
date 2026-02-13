/**
 * @file molt_skill.hpp
 * @brief MoltCat skill extension interfaces
 * @note Plugin developers need to implement these interfaces to extend MoltCat functionality
 * @warning Uses plugin interface types to ensure ABI compatibility across DLL boundaries
 */

#pragma once

#include "molt_model.hpp"

namespace moltcat {

// ============================================================
// Skill metadata
// ============================================================

/**
 * @brief Skill metadata
 *
 * Uses IString* to ensure ABI compatibility, framework manages lifecycle.
 */
struct SkillMetadata {
    IString* name = nullptr;                  // Skill name (framework managed)
    IString* version = nullptr;               // Version (framework managed)
    IString* description = nullptr;           // Description (framework managed)
    IString* author = nullptr;                // Author (framework managed, can be nullptr)
    IList<IString*>* supported_task_types = nullptr;  // Supported task types list (framework managed)

    // Convenience access
    auto get_name() const noexcept -> const char* {
        return name ? name->c_str() : "";
    }

    auto get_version() const noexcept -> const char* {
        return version ? version->c_str() : "";
    }

    auto get_description() const noexcept -> const char* {
        return description ? description->c_str() : "";
    }

    auto get_author() const noexcept -> const char* {
        return author ? author->c_str() : "";
    }
};

// ============================================================
// Skill interface
// ============================================================

/**
 * @brief Skill interface
 *
 * Plugin developers: Inherit from this interface and implement all virtual functions
 * to create custom skill plugins.
 *
 * @note
 * - All IString* parameters are managed by framework, plugins must not free
 * - When returning IString*, plugins are responsible for creating, framework for freeing
 * - Avoid using STL containers and std::string to ensure ABI compatibility
 *
 * @example
 * @code
 * class MySkill : public moltcat::IMoltSkill {
 * public:
 *     auto name() const -> const char* override { return "MySkill"; }
 *     auto version() const -> const char* override { return "1.0.0"; }
 *     auto description() const -> const char* override { return "My skill"; }
 *
 *     auto can_handle(const char* task_type) const -> bool override {
 *         return strcmp(task_type, "my_task") == 0;
 *     }
 *
 *     auto supported_task_types() -> IList<IString*>* override {
 *         auto list = IList<IString*>::create();
 *         list->add(IString::create("my_task"));
 *         return list;
 *     }
 *
 *     auto execute(const MoltTask& task, MoltContext& ctx) -> MoltResult override {
 *         MoltResult result;
 *         result.success = true;
 *         result.data = IString::create(R"({"message": "Done"})");
 *         return result;
 *     }
 * };
 * @endcode
 */
class IMoltSkill {
public:
    virtual ~IMoltSkill() = default;

    /**
     * @brief Get skill name
     * @return Skill name for identification and logging
     */
    virtual auto name() const -> const char* = 0;

    /**
     * @brief Get skill version
     * @return Version string (following semantic versioning)
     */
    virtual auto version() const -> const char* = 0;

    /**
     * @brief Get skill description
     * @return Skill functionality description
     */
    virtual auto description() const -> const char* = 0;

    /**
     * @brief Get skill author
     * @return Author name, can be nullptr
     */
    virtual auto author() const -> const char* { return nullptr; }

    /**
     * @brief Get skill metadata
     * @return Skill metadata structure
     * @note Framework is responsible for freeing strings in returned metadata
     */
    virtual auto metadata() -> SkillMetadata {
        SkillMetadata meta;
        meta.name = IString::create(name());
        meta.version = IString::create(version());
        meta.description = IString::create(description());
        const char* auth = author();
        if (auth) meta.author = IString::create(auth);
        meta.supported_task_types = supported_task_types();
        return meta;
    }

    /**
     * @brief Check if skill can handle specified task type
     * @param task_type Task type identifier (C string, framework managed)
     * @return Returns true if this skill can handle the task type
     */
    virtual auto can_handle(const char* task_type) const -> bool = 0;

    /**
     * @brief Get list of supported task types
     * @return List of supported task types, caller responsible for destroying list (but not strings)
     *
     * @note IString* in list are created by plugin, freed by framework when destroying list
     */
    virtual auto supported_task_types() -> IList<IString*>* = 0;

    /**
     * @brief Execute task
     * @param task Task to execute (framework managed, valid during function call)
     * @param ctx Execution context (framework managed, valid during function call)
     * @return Execution result
     *
     * @note
     * - data and error_message in result are created by plugin, freed by framework
     * - Pointers in task and context become invalid after function returns
     */
    virtual auto execute(const MoltTask& task, MoltContext& ctx) -> MoltResult = 0;

    /**
     * @brief Initialize skill
     * @return Returns true if initialization successful
     * @note Called once after skill is loaded, for initializing resources
     */
    virtual auto initialize() -> bool { return true; }

    /**
     * @brief Shutdown skill
     * @note Called before skill is unloaded, for releasing resources
     */
    virtual void shutdown() {}

    /**
     * @brief Pause skill
     * @note Stop receiving new tasks, but don't unload
     */
    virtual void pause() {}

    /**
     * @brief Resume skill
     * @note Resume receiving new tasks
     */
    virtual void resume() {}

    /**
     * @brief Get skill status
     * @return Status string (freed by framework)
     */
    virtual auto get_status() -> IString* {
        return IString::create("running");
    }

    /**
     * @brief Health check
     * @return Returns true if healthy
     */
    virtual auto health_check() const -> bool {
        return true;
    }

    /**
     * @brief Get skill configuration schema
     * @return JSON format configuration schema (freed by framework), can be nullptr
     */
    virtual auto get_config_schema() -> IString* {
        return nullptr;
    }

    /**
     * @brief Validate configuration
     * @param config_json JSON format configuration (framework managed)
     * @return Validation result, error message in error_message (freed by framework)
     */
    virtual auto validate_config(const char* config_json) -> MoltResult {
        (void)config_json;
        MoltResult result;
        result.success = true;
        result.data = IString::create("{}");
        return result;
    }
};

// ============================================================
// Skill factory interface
// ============================================================

/**
 * @brief Skill factory interface
 *
 * Plugin developers: Implement this interface to create skill instances.
 * MoltCat uses factory pattern to dynamically create skill instances.
 *
 * @note Factory itself is managed by framework, plugins only need to implement interface
 */
class IMoltSkillFactory {
public:
    virtual ~IMoltSkillFactory() = default;

    /**
     * @brief Create skill instance
     * @return Raw pointer to skill object, framework responsible for destruction
     */
    virtual auto create() -> IMoltSkill* = 0;

    /**
     * @brief Destroy skill instance
     * @param skill Skill pointer to destroy
     */
    virtual void destroy(IMoltSkill* skill) = 0;

    /**
     * @brief Get skill type name
     * @return Type identifier
     */
    virtual auto type_name() const -> const char* = 0;

    /**
     * @brief Get skill metadata (without instantiation)
     * @return Skill metadata
     */
    virtual auto get_metadata() const -> SkillMetadata = 0;

    /**
     * @brief Get factory supported API version
     * @return API version string
     */
    virtual auto get_api_version() const -> const char* {
        return "1.0.0";
    }

    /**
     * @brief Check API compatibility
     * @param required_version Required API version
     * @return Returns true if compatible
     */
    virtual auto is_compatible(const char* required_version) const -> bool {
        (void)required_version;
        return true;  // Simplified implementation, should actually compare versions
    }
};

// ============================================================
// Plugin information
// ============================================================

/**
 * @brief Plugin information structure
 *
 * Plugin developers: Return this structure in export function.
 */
struct PluginInfo {
    IString* name = nullptr;              // Plugin name
    IString* version = nullptr;           // Plugin version
    IString* description = nullptr;       // Plugin description
    IString* author = nullptr;            // Plugin author
    IString* license = nullptr;           // License
    IString* homepage = nullptr;          // Homepage URL (can be nullptr)

    // Convenience access
    auto get_name() const noexcept -> const char* {
        return name ? name->c_str() : "";
    }

    auto get_version() const noexcept -> const char* {
        return version ? version->c_str() : "";
    }

    auto get_description() const noexcept -> const char* {
        return description ? description->c_str() : "";
    }

    auto get_author() const noexcept -> const char* {
        return author ? author->c_str() : "";
    }

    auto get_license() const noexcept -> const char* {
        return license ? license->c_str() : "";
    }

    auto get_homepage() const noexcept -> const char* {
        return homepage ? homepage->c_str() : "";
    }
};

} // namespace moltcat

// ============================================================
// Plugin Export
// ============================================================

/**
 * @file molt_export.hpp
 * @brief Plugin export definitions and macros
 *
 * Plugin developers should include this file in their plugin implementation
 * to use the MOLT_EXPORT_SKILL macro.
 *
 * @note
 * - On Windows: Define MOLTCAT_BUILD when building the plugin DLL
 * - On POSIX: Use -fvisibility=hidden and export only the C API functions
 *
 * @example
 * @code
 * // MySkillPlugin.cpp
 * #include <moltcat/molt_export.hpp>
 * #include "MySkillFactory.h"
 *
 * MOLT_EXPORT_SKILL(MySkillFactory, "MySkill", "1.0.0")
 * @endcode
 */
