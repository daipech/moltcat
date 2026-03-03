#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <map>
#include "../gateway/ws_gateway.hpp"  // 引入 ConnectionInfo
#include "../utils/error.hpp"

namespace moltcat::gateway::auth {

/**
 * @brief Permission checker
 *
 * Responsible for checking client permissions:
 * - Scope permissions (operator.read, operator.write, etc.)
 * - Cap permissions (code-generation, code-review, etc.)
 * - Command permissions (task.execute, task.cancel, etc.)
 * - Permission permissions (fine-grained boolean permissions)
 */
class PermissionChecker {
public:
    /**
     * @brief Method to permission mapping
     */
    struct MethodPermission {
        std::string required_scope;      // Required scope (optional)
        std::string required_cap;        // Required cap (optional)
        std::string required_command;    // Required command (optional)
        std::string required_permission; // Required permission (optional)
    };

    /**
     * @brief Constructor
     */
    PermissionChecker() = default;

    ~PermissionChecker() = default;

    /**
     * @brief Check if has permission to execute method
     *
     * @param conn Connection information
     * @param method Method name
     * @return Result<bool> true indicates has permission, false indicates no permission, or error
     */
    [[nodiscard]] auto check_method_permission(
        const ConnectionInfo& conn,
        std::string_view method
    ) -> Result<bool>;

    /**
     * @brief Check Scope permission
     *
     * @param conn Connection information
     * @param required_scope Required scope
     * @return bool Whether has permission
     */
    [[nodiscard]] auto check_scope(
        const ConnectionInfo& conn,
        std::string_view required_scope
    ) -> bool;

    /**
     * @brief Check Cap permission
     *
     * @param conn Connection information
     * @param required_cap Required cap
     * @return bool Whether has permission
     */
    [[nodiscard]] auto check_cap(
        const ConnectionInfo& conn,
        std::string_view required_cap
    ) -> bool;

    /**
     * @brief Check Command permission
     *
     * @param conn Connection information
     * @param command Command name
     * @return bool Whether has permission
     */
    [[nodiscard]] auto check_command(
        const ConnectionInfo& conn,
        std::string_view command
    ) -> bool;

    /**
     * @brief Check Permission permission
     *
     * @param conn Connection information
     * @param permission Permission name
     * @return bool Whether has permission
     */
    [[nodiscard]] auto check_permission(
        const ConnectionInfo& conn,
        std::string_view permission
    ) -> bool;

    /**
     * @brief Check if operator role
     *
     * @param conn Connection information
     * @return bool Whether is operator
     */
    [[nodiscard]] static auto is_operator(const ConnectionInfo& conn) -> bool;

    /**
     * @brief Check if node role
     *
     * @param conn Connection information
     * @return bool Whether is node
     */
    [[nodiscard]] static auto is_node(const ConnectionInfo& conn) -> bool;

    /**
     * @brief Register method permission mapping
     *
     * @param method Method name
     * @param perm Permission requirement
     */
    auto register_method_permission(std::string_view method, const MethodPermission& perm)
        -> void;

    /**
     * @brief Get method permission requirement
     *
     * @param method Method name
     * @return std::optional<MethodPermission> Permission requirement, returns nullopt if not found
     */
    [[nodiscard]] auto get_method_permission(std::string_view method) const
        -> std::optional<MethodPermission>;

private:
    // Method permission mapping
    std::unordered_map<std::string, MethodPermission> method_permissions_;

    /**
     * @brief Check wildcard matching
     *
     * @param pattern Pattern (e.g., "operator.*")
     * @param value Value (e.g., "operator.read")
     * @return bool Whether matches
     */
    [[nodiscard]] static auto match_wildcard(std::string_view pattern, std::string_view value)
        -> bool;
};

} // namespace moltcat::gateway::auth
