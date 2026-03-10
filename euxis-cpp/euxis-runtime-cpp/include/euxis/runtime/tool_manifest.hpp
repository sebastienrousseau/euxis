/// @file
/// @brief declarative manifest for agent-accessible tools.
#pragma once

#include <expected>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::runtime {

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

} // namespace euxis::runtime
