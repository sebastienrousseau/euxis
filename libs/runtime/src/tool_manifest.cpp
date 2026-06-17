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

// classify_approval() recognises the following read-only verb prefixes,
// dispatched by ASCII-folded first byte below: check / count (c),
// describe (d), fetch (f), get (g), list / lookup (l), query (q),
// read (r), show / status / scan / search (s), validate / view (v).
// Names are case-insensitive ASCII; non-ASCII names fall through to
// ExecCapable.

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

    // 2. First-byte dispatch into a small per-letter sub-table.
    //
    // The original implementation walked the full 15-prefix readonly
    // verb table on every classification, which made ExecCapable
    // (no match, full traversal) ~3x slower than Readonly in the
    // microbench. Dispatching on the ASCII-folded first byte cuts
    // the worst-case scan to 4 prefixes and turns the no-match path
    // into a direct return — verbs starting with `w` (write_*),
    // `e` (exec_*), `r` not followed by `read*`, etc., now skip the
    // table entirely.
    const std::string_view name{decl.name};
    if (name.empty()) return ApprovalClass::ExecCapable;

    const auto check = [&](std::string_view prefix) noexcept -> bool {
        if (!starts_with_ci(name, prefix)) return false;
        // Word boundary: end-of-name or '_' to reject false positives
        // like "getup_destructive_action".
        return name.size() == prefix.size() || name[prefix.size()] == '_';
    };

    const char c0 = name[0];
    const char first = (c0 >= 'A' && c0 <= 'Z')
        ? static_cast<char>(c0 + 32) : c0;

    switch (first) {
        case 'c':
            if (check("check") || check("count")) return ApprovalClass::Readonly;
            break;
        case 'd':
            if (check("describe")) return ApprovalClass::Readonly;
            break;
        case 'f':
            if (check("fetch")) return ApprovalClass::Readonly;
            break;
        case 'g':
            if (check("get")) return ApprovalClass::Readonly;
            break;
        case 'l':
            if (check("list") || check("lookup")) return ApprovalClass::Readonly;
            break;
        case 'q':
            if (check("query")) return ApprovalClass::Readonly;
            break;
        case 'r':
            if (check("read")) return ApprovalClass::Readonly;
            break;
        case 's':
            if (check("show") || check("status") ||
                check("scan") || check("search")) return ApprovalClass::Readonly;
            break;
        case 'v':
            if (check("validate") || check("view")) return ApprovalClass::Readonly;
            break;
        default:
            break;
    }

    // 3. Conservative fallback.
    return ApprovalClass::ExecCapable;
}

} // namespace euxis::runtime
