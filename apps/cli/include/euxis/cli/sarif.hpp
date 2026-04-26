/// @file
/// @brief SARIF v2.1.0 output formatter for verdict findings.
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace euxis::cli {

/// A single finding that can be converted to SARIF.
struct SarifFinding {
    std::string rule_id;         // e.g. "euxis/security-01"
    std::string message;
    std::string level;           // "error", "warning", "note"
    std::string file_path;       // optional — file location
    int line{0};                 // optional — line number
    std::string agent_id;        // which agent produced this
    std::string pillar;          // evidence pillar
};

/// Convert verdict findings to SARIF v2.1.0 JSON.
[[nodiscard]] auto findings_to_sarif(
    const std::vector<SarifFinding>& findings,
    const std::string& tool_version = "0.0.10") -> nlohmann::json;

/// Parse agent evidence output to extract SarifFinding objects.
/// Attempts to extract file paths and line numbers from findings text.
[[nodiscard]] auto extract_sarif_findings(
    const std::string& agent_output,
    const std::string& agent_id,
    const std::string& pillar,
    const std::string& verdict) -> std::vector<SarifFinding>;

} // namespace euxis::cli
