/// @file
/// @brief declarative manifest for agent-accessible tools.
#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::runtime {

/// @brief Capability class used to gate approval prompts at runtime.
///
/// Mirrors the OpenClaw ACP approval-classifier pattern: a tool call is
/// classified once into one of three bands, and the gateway approves all
/// calls within the same band + path + instruction tuple in one prompt
/// rather than per-invocation. The classifier is conservative — when in
/// doubt it returns ExecCapable, not Readonly.
enum class ApprovalClass {
    /// Read-only or query operations that cannot mutate state outside the
    /// process (list, get, query, show, status, …).
    Readonly,
    /// Operations that can mutate the filesystem, run a process, or write
    /// to the network (write, exec, run, shell, delete, create, …).
    ExecCapable,
    /// Operations that affect the agent runtime itself or the gateway's
    /// control plane (admin, policy, session-control, …).
    ControlPlane,
};

/// @brief Render an ApprovalClass as a lower-case stable string.
[[nodiscard]] constexpr auto approval_class_name(ApprovalClass c) noexcept
    -> std::string_view {
    switch (c) {
        case ApprovalClass::Readonly:    return "readonly";
        case ApprovalClass::ExecCapable: return "exec_capable";
        case ApprovalClass::ControlPlane: return "control_plane";
    }
    return "exec_capable"; // unreachable; conservative fallback
}

/// @brief semantic declaration of a tool's capabilities.
struct ToolDeclaration_v2 {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
};

/// @brief A single tool entry in the manifest with security and source info.
struct ToolManifestEntry {
    ToolDeclaration_v2 declaration;
    std::string source;
    std::string required_scope;
    bool requires_approval{false};
};

/// @brief complete set of tools available to an agent.
struct ToolManifest {
    std::string agent_id;
    std::string version;
    std::vector<ToolManifestEntry> tools;
};

/// @brief Load a tool manifest from a JSON file.
auto load_tool_manifest(const std::string& path)
    -> std::expected<ToolManifest, std::string>;

/// @brief merge two tool manifests, with the overlay taking precedence.
auto merge_manifests(const ToolManifest& base,
                      const ToolManifest& overlay) -> ToolManifest;

/// @brief Validate manifest against security scopes.
auto validate_tool_manifest(const ToolManifest& manifest,
                             const std::vector<std::string>& available_scopes)
    -> std::expected<void, std::string>;

/// @brief Classify a tool declaration by name + optional required scope.
///
/// Heuristics, in order:
///   1. If @p required_scope contains "admin", "control", or "policy" →
///      ControlPlane.
///   2. If the declaration name matches a known read-only verb prefix
///      (read, list, get, query, show, status, scan, fetch, describe,
///      view, check, validate, lookup, search, count) → Readonly.
///   3. Otherwise → ExecCapable (conservative default).
///
/// The classifier is intentionally pure and lookup-free at runtime so it
/// can be called on every tool invocation without measurable overhead.
[[nodiscard]] auto classify_approval(const ToolDeclaration_v2& decl,
                                     std::string_view required_scope = "")
    noexcept -> ApprovalClass;

} // namespace euxis::runtime
