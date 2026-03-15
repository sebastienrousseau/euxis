/// @file
/// @brief Certification readiness assessment engine — data model and core functions.
#pragma once

#include "euxis/cli/command.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace euxis::cli::certification {

// --- Framework ---

enum class Framework { General, SOC2, ISO27001 };

auto framework_name(Framework f) -> std::string;
auto parse_framework(const std::string& s) -> std::optional<Framework>;

// --- Data structures ---

struct GateResult {
    std::string name;
    std::string status;       // "pass", "fail", "partial", "skipped"
    std::string detail;
    bool blocking{false};
    nlohmann::json data;
};

struct DomainResult {
    std::string name;
    std::string status;       // "verified", "gaps", "blocked", "inconclusive"
    double coverage{0.0};
    int evidence_count{0};
    bool critical{false};
    std::vector<std::string> findings;
    std::vector<std::string> missing_controls;
};

struct Evidence {
    std::string id;
    std::string domain;
    std::string source;       // "file", "git", "command", "artifact", "builtin"
    std::string status;       // "pass", "fail", "partial", "inferred"
    bool execution_backed{false};
    std::string summary;
};

struct DomainDefinition {
    std::string name;
    std::vector<std::string> required_signals;
    std::vector<std::string> optional_signals;
    bool critical{false};
};

struct QualityRisk {
    std::string status;       // "pass", "gaps"
    std::vector<std::string> high_complexity_files;
    std::vector<std::string> hotspots;
    nlohmann::json data;
};

struct RunOptions {
    Framework framework{Framework::General};
    bool strict{false};
    bool run_build{true};
    bool run_tests{true};
    bool run_security{true};
    int commit_window{20};
    std::string since_ref;
};

struct RunResult {
    Framework framework{Framework::General};
    std::string target;
    bool strict{false};

    std::vector<GateResult> gates;
    std::vector<DomainResult> domains;
    std::vector<Evidence> evidence;
    QualityRisk quality_risk;

    std::vector<std::string> blocking_issues;
    std::vector<std::string> major_gaps;
    std::vector<std::string> recommended_actions;
    std::vector<std::string> skipped_gates;

    std::string status;       // "READY", "READY WITH GAPS", "BLOCKED", "INCONCLUSIVE"
    int confidence{0};
    int exit_code{0};
    std::string readiness_summary;

    auto to_json() const -> nlohmann::json;
};

// --- Core functions ---

/// Return the 18 certification domain definitions.
auto default_domains() -> std::vector<DomainDefinition>;

/// Apply framework overlay (adjusts which domains are critical).
void apply_framework_overlay(Framework f, std::vector<DomainDefinition>& domains);

/// Collect evidence and evaluate all gates + domains.
auto run_certification(Context& ctx,
                       const std::filesystem::path& target,
                       const RunOptions& opts) -> RunResult;

} // namespace euxis::cli::certification
