/// @file
/// @brief Value types for libs/vulndb — OSV vulnerability records,
///        per-component match results, and the enriched report shape.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/sbom/openvex.hpp"

namespace euxis::vulndb {

/// @brief Severity bands used across the OSV schema. Maps to CVSS v3.x
///        severity buckets so downstream code can branch by category
///        without parsing a numeric vector.
enum class Severity : std::uint8_t {
    Unknown,
    Low,
    Medium,
    High,
    Critical,
};

/// @brief A single OSV vulnerability record (subset).
///
/// OSV's full schema is large; we keep the fields the enricher actually
/// consumes plus a `raw` JSON copy so callers can pull anything else
/// they need without re-parsing.
struct OsvVuln {
    /// Primary identifier — GHSA-xxxx, CVE-XXXX-XXXX, GO-XXXX-XXXX, etc.
    std::string  id;
    /// Aliases (e.g. GHSA <-> CVE pairing).
    std::vector<std::string> aliases;
    /// One-line human summary from OSV.
    std::string  summary;
    /// Long-form description (may contain markdown).
    std::string  details;
    /// First patched version, when one exists. Empty when unfixed.
    std::string  fixed_in;
    /// Vulnerable version range. Free-form per ecosystem (e.g.
    /// ">=1.0.0, <1.5.10" or "<3.0.0").
    std::string  vulnerable_range;
    /// Best-effort severity bucket derived from OSV's `severity[]`
    /// array. Critical/High/Medium/Low when a CVSS vector exists,
    /// Unknown otherwise.
    Severity     severity{Severity::Unknown};
    /// CVSS v3 score, when published. 0.0 when not present.
    double       cvss_score{0.0};
    /// OSV `references[]` — vendor advisories, patches, exploit POCs.
    std::vector<std::string> references;
    /// The raw OSV JSON object the record was parsed from. Useful for
    /// downstream tooling that needs fields this struct doesn't model.
    nlohmann::json raw;
};

/// @brief Per-component enrichment result.
///
/// Every `Component` from the input SBOM gets at most one `VulnMatch`
/// entry per vulnerability. A component with zero matches still gets
/// a record (with empty `vulns`) so consumers can iterate all
/// components uniformly.
struct VulnMatch {
    /// Echo of the originating component's PURL string for join.
    std::string  purl;
    /// Echo of `Component::name` for human-readable reports.
    std::string  component_name;
    /// Echo of `Component::version`.
    std::string  component_version;
    /// Vulnerabilities OSV reported as affecting this exact version.
    std::vector<OsvVuln> vulns;
};

/// @brief The full enrichment output.
///
/// `matches[i]` is the per-component result for the i-th input
/// component (same order as `SbomDocument::components`). `vex` is an
/// auto-generated OpenVEX document where every affected component
/// becomes a `VexStatement{Affected}` and every clean component
/// becomes a `VexStatement{NotAffected}` for telemetry purposes.
struct EnrichedReport {
    std::vector<VulnMatch>            matches;
    euxis::sbom::VexDocument          vex;

    /// True when at least one matched vulnerability had severity High
    /// or Critical. Useful as a CI gate.
    [[nodiscard]] auto has_high_or_critical() const noexcept -> bool {
        for (const auto& m : matches) {
            for (const auto& v : m.vulns) {
                if (v.severity == Severity::High ||
                    v.severity == Severity::Critical) {
                    return true;
                }
            }
        }
        return false;
    }
};

/// @brief Render severity as a stable string for SARIF / telemetry.
[[nodiscard]] inline auto severity_name(Severity s) noexcept -> std::string_view {
    switch (s) {
        case Severity::Unknown:  return "unknown";
        case Severity::Low:      return "low";
        case Severity::Medium:   return "medium";
        case Severity::High:     return "high";
        case Severity::Critical: return "critical";
    }
    return "unknown";
}

/// @brief Derive a `Severity` band from a CVSS v3 base score.
///
/// Buckets follow the NVD convention:
///   0.0          -> Unknown (no score)
///   0.1 - 3.9    -> Low
///   4.0 - 6.9    -> Medium
///   7.0 - 8.9    -> High
///   9.0 - 10.0   -> Critical
[[nodiscard]] inline auto severity_from_cvss(double score) noexcept -> Severity {
    if (score <= 0.0)  return Severity::Unknown;
    if (score < 4.0)   return Severity::Low;
    if (score < 7.0)   return Severity::Medium;
    if (score < 9.0)   return Severity::High;
    return Severity::Critical;
}

} // namespace euxis::vulndb
