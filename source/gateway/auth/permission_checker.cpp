#include "permission_checker.hpp"
#include "../utils/string_utils.hpp"
#include <algorithm>

namespace moltcat::gateway::auth {

// ==================== Inline Helper Functions ====================

// For compilation, ConnectionInfo is defined in ws_gateway.hpp
// Here we define a simplified version in current file for compilation

// ==================== PermissionChecker Implementation ====================

auto PermissionChecker::check_method_permission(
    const ConnectionInfo& conn,
    std::string_view method
) -> Result<bool> {
    // Find method permission requirement
    auto perm_opt = get_method_permission(method);
    if (!perm_opt.has_value()) {
        // Unregistered method, allow by default (or can deny by default)
        return Ok(true);
    }

    auto& perm = perm_opt.value();

    // Check Scope
    if (!perm.required_scope.empty()) {
        if (!check_scope(conn, perm.required_scope)) {
            return Ok(false);
        }
    }

    // Check Cap
    if (!perm.required_cap.empty()) {
        if (!check_cap(conn, perm.required_cap)) {
            return Ok(false);
        }
    }

    // Check Command
    if (!perm.required_command.empty()) {
        if (!check_command(conn, perm.required_command)) {
            return Ok(false);
        }
    }

    // Check Permission
    if (!perm.required_permission.empty()) {
        if (!check_permission(conn, perm.required_permission)) {
            return Ok(false);
        }
    }

    return Ok(true);
}

auto PermissionChecker::check_scope(
    const ConnectionInfo& conn,
    std::string_view required_scope
) -> bool {
    // Check exact match
    if (conn.scopes.end() != std::find(conn.scopes.begin(), conn.scopes.end(), required_scope)) {
        return true;
    }

    // Check wildcard matching
    for (const auto& scope : conn.scopes) {
        if (match_wildcard(scope, required_scope)) {
            return true;
        }
    }

    return false;
}

auto PermissionChecker::check_cap(
    const ConnectionInfo& conn,
    std::string_view required_cap
) -> bool {
    // Check if has required cap
    return conn.caps.end() != std::find(conn.caps.begin(), conn.caps.end(), required_cap);
}

auto PermissionChecker::check_command(
    const ConnectionInfo& conn,
    std::string_view command
) -> bool {
    // Check if command is in whitelist
    return conn.commands.end() !=
        std::find(conn.commands.begin(), conn.commands.end(), command);
}

auto PermissionChecker::check_permission(
    const ConnectionInfo& conn,
    std::string_view permission
) -> bool {
    // Check fine-grained permission
    auto it = conn.permissions.find(std::string(permission));
    if (it != conn.permissions.end()) {
        return it->second;
    }

    // Default deny
    return false;
}

auto PermissionChecker::is_operator(const ConnectionInfo& conn) -> bool {
    return conn.role == "operator";
}

auto PermissionChecker::is_node(const ConnectionInfo& conn) -> bool {
    return conn.role == "node";
}

auto PermissionChecker::register_method_permission(
    std::string_view method,
    const MethodPermission& perm
) -> void {
    method_permissions_[std::string(method)] = perm;
}

auto PermissionChecker::get_method_permission(std::string_view method) const
    -> std::optional<MethodPermission> {
    auto it = method_permissions_.find(std::string(method));
    if (it != method_permissions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto PermissionChecker::match_wildcard(std::string_view pattern, std::string_view value) -> bool {
    // Simple wildcard matching, supports "*"
    // Example: "operator.*" matches "operator.read", "operator.write"

    if (pattern.empty()) {
        return value.empty();
    }

    // Find wildcard position
    auto wildcard_pos = pattern.find('*');

    if (wildcard_pos == std::string_view::npos) {
        // No wildcard, exact match
        return pattern == value;
    }

    // Check prefix
    if (wildcard_pos > 0) {
        if (value.size() < wildcard_pos) {
            return false;
        }
        if (value.substr(0, wildcard_pos) != pattern.substr(0, wildcard_pos)) {
            return false;
        }
    }

    // Check suffix
    if (wildcard_pos < pattern.size() - 1) {
        auto suffix = pattern.substr(wildcard_pos + 1);
        if (value.size() < suffix.size()) {
            return false;
        }
        if (value.substr(value.size() - suffix.size()) != suffix) {
            return false;
        }
    }

    return true;
}

} // namespace moltcat::gateway::auth
