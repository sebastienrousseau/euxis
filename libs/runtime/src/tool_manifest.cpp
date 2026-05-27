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

namespace {

// Read-only verb prefixes recognised by classify_approval(). The check is
// case-insensitive ASCII; non-ASCII names fall through to ExecCapable.
constexpr std::string_view kReadonlyVerbs[] = {
    "read", "list", "get", "query", "show", "status",
    "scan", "fetch", "describe", "view", "check",
    "validate", "lookup", "search", "count",
};

[[nodiscard]] auto starts_with_ci(std::string_view s, std::string_view prefix) noexcept
    -> bool {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        const char a = s[i];
        const char b = prefix[i];
        const char la = (a >= 'A' && a <= 'Z') ? static_cast<char>(a + 32) : a;
        const char lb = (b >= 'A' && b <= 'Z') ? static_cast<char>(b + 32) : b;
        if (la != lb) return false;
    }
    return true;
}

[[nodiscard]] auto contains_ci(std::string_view s, std::string_view needle) noexcept
    -> bool {
    if (needle.empty() || s.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= s.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            const char a = s[i + j];
            const char b = needle[j];
            const char la = (a >= 'A' && a <= 'Z') ? static_cast<char>(a + 32) : a;
            const char lb = (b >= 'A' && b <= 'Z') ? static_cast<char>(b + 32) : b;
            if (la != lb) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

} // namespace

auto classify_approval(const ToolDeclaration_v2& decl,
                       std::string_view required_scope) noexcept
    -> ApprovalClass {
    // 1. Scope override: anything in admin / control / policy land is
    //    ControlPlane regardless of the name.
    if (contains_ci(required_scope, "admin") ||
        contains_ci(required_scope, "control") ||
        contains_ci(required_scope, "policy")) {
        return ApprovalClass::ControlPlane;
    }

    // 2. Name-prefix match against the read-only verb table.
    const std::string_view name{decl.name};
    for (const auto verb : kReadonlyVerbs) {
        if (starts_with_ci(name, verb)) {
            // Reject false positives like "getup_destructive_action".
            // A read verb followed by '_' or end-of-name counts as readonly;
            // anything else falls through to ExecCapable.
            if (name.size() == verb.size() || name[verb.size()] == '_') {
                return ApprovalClass::Readonly;
            }
        }
    }

    // 3. Conservative fallback.
    return ApprovalClass::ExecCapable;
}

} // namespace euxis::runtime
