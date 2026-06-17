/// @file
/// @brief SARIF v2.1.0 output formatter.
///
/// Emits SARIF documents that consumer tools (GitHub Code Scanning,
/// Defender for Cloud, GitLab SAST widget, Snyk import) can ingest
/// directly. Carries CWE Top 25:2025 and OWASP Top 10:2025 taxonomies
/// as per the 2025-12-11 CISA release and the 2025-11 OWASP release.
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "euxis/security/finding.hpp"

namespace euxis::cli {

/// Legacy SARIF finding struct (kept for the agent-output regex path
/// used by `extract_sarif_findings`). New emitters should produce
/// `euxis::security::Finding` and call `findings_to_sarif()`.
struct SarifFinding {
    std::string rule_id;         // e.g. "euxis/security-01"
    std::string message;
    std::string level;           // "error", "warning", "note"
    std::string file_path;       // optional — file location
    int line{0};                 // optional — line number
    std::string agent_id;        // which agent produced this
    std::string pillar;          // evidence pillar
};

/// Convert legacy verdict findings to SARIF v2.1.0 JSON.
/// Preserved for back-compat with the agent-driven verification path.
[[nodiscard]] auto findings_to_sarif(
    const std::vector<SarifFinding>& findings,
    const std::string& tool_version = "0.1.2") -> nlohmann::json;

/// Convert the canonical Finding type to SARIF v2.1.0 JSON, including
/// `taxonomies` entries for CWE Top 25:2025 and OWASP Top 10:2025, and
/// `fixes[]` blocks for any deterministic codemods.
[[nodiscard]] auto findings_to_sarif(
    const std::vector<euxis::security::Finding>& findings,
    const std::string& tool_version = "0.1.2") -> nlohmann::json;

/// Parse agent evidence output to extract SarifFinding objects.
/// Attempts to extract file paths and line numbers from findings text.
[[nodiscard]] auto extract_sarif_findings(
    const std::string& agent_output,
    const std::string& agent_id,
    const std::string& pillar,
    const std::string& verdict) -> std::vector<SarifFinding>;

} // namespace euxis::cli
