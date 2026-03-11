#include "euxis/runtime/tool_manifest.hpp"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

namespace euxis::runtime {

auto load_tool_manifest(const std::string& path)
    -> std::expected<ToolManifest, std::string> {
    std::ifstream f(path);
    if (!f.is_open()) {
        return std::unexpected("Failed to open tool manifest: " + path);
    }

    auto j = nlohmann::json::parse(f, nullptr, false);
    if (j.is_discarded()) {
        return std::unexpected("Failed to parse tool manifest: " + path);
    }

    ToolManifest manifest;
    manifest.agent_id = j.value("agent_id", "");
    manifest.version = j.value("version", "");

    for (const auto& tj : j.value("tools", nlohmann::json::array())) {
        ToolManifestEntry entry;
        entry.declaration.name = tj.value("name", "");
        entry.declaration.description = tj.value("description", "");
        entry.declaration.input_schema = tj.value("input_schema", nlohmann::json{});
        entry.source = tj.value("source", "");
        entry.required_scope = tj.value("required_scope", "");
        entry.requires_approval = tj.value("requires_approval", false);
        manifest.tools.push_back(std::move(entry));
    }

    return manifest;
}

auto merge_manifests(const ToolManifest& base,
                      const ToolManifest& overlay) -> ToolManifest {
    ToolManifest result = base;
    result.version = overlay.version.empty() ? base.version : overlay.version;

    for (const auto& overlay_tool : overlay.tools) {
        auto it = std::ranges::find_if(result.tools,
            [&](const ToolManifestEntry& e) {
                return e.declaration.name == overlay_tool.declaration.name;
            });
        if (it != result.tools.end()) {
            *it = overlay_tool;
        } else {
            result.tools.push_back(overlay_tool);
        }
    }

    return result;
}

auto validate_tool_manifest(const ToolManifest& manifest,
                             const std::vector<std::string>& available_scopes)
    -> std::expected<void, std::string> {
    for (const auto& entry : manifest.tools) {
        if (entry.declaration.name.empty()) {
            return std::unexpected("Tool manifest entry has empty name");
        }
        if (!entry.required_scope.empty()) {
            auto it = std::ranges::find(available_scopes, entry.required_scope);
            if (it == available_scopes.end()) {
                return std::unexpected(
                    "Tool '" + entry.declaration.name +
                    "' requires scope '" + entry.required_scope +
                    "' which is not available");
            }
        }
    }
    return {};
}

} // namespace euxis::runtime
