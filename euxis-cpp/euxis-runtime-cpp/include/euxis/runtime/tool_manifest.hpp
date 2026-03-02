#pragma once

#include <expected>
#include <string>
#include <vector>

#include "tool.hpp"

namespace euxis::runtime {

struct ToolManifestEntry {
    ToolDeclaration declaration;
    std::string source;          // e.g. "builtin", "plugin:skill-x"
    std::string required_scope;  // authorization scope needed
    bool requires_approval{false};
};

struct ToolManifest {
    std::string agent_id;
    std::string version;
    std::vector<ToolManifestEntry> tools;
};

/// Load a tool manifest from a JSON file.
[[nodiscard]] auto load_tool_manifest(const std::string& path)
    -> std::expected<ToolManifest, std::string>;

/// Merge an overlay manifest into a base manifest (overlay tools override base by name).
[[nodiscard]] auto merge_manifests(const ToolManifest& base,
                                    const ToolManifest& overlay) -> ToolManifest;

/// Validate that all required scopes are present.
[[nodiscard]] auto validate_tool_manifest(const ToolManifest& manifest,
                                           const std::vector<std::string>& available_scopes)
    -> std::expected<void, std::string>;

} // namespace euxis::runtime
