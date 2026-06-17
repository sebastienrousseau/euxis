/// @file
/// @brief OpenVEX statement emitter.
///
/// VEX (Vulnerability Exploitability eXchange) tells downstream
/// consumers when a CVE in an SBOM dependency is NOT exploitable
/// in the product's actual call path. The OpenVEX spec is at
/// https://github.com/openvex/spec. Used by the CRA reporting
/// pipeline (P0.6) and the reachability pruning stage (P1.7).
#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace euxis::sbom {

/// VEX status per OpenVEX §status enum.
enum class VexStatus {
    NotAffected,
    Affected,
    Fixed,
    UnderInvestigation,
};

/// Justification when status == NotAffected. OpenVEX §justification.
enum class VexJustification {
    None,
    ComponentNotPresent,
    VulnerableCodeNotPresent,
    VulnerableCodeNotInExecutePath,
    VulnerableCodeCannotBeControlledByAdversary,
    InlineMitigationsAlreadyExist,
};

struct VexStatement {
    /// Vulnerability id (CVE-YYYY-N or GHSA-x-x-x or OSV id).
    std::string vulnerability_name;

    /// Products affected: purl strings. OpenVEX §products.
    std::vector<std::string> product_purls;

    VexStatus status{VexStatus::UnderInvestigation};

    /// Required when status == NotAffected.
    VexJustification justification{VexJustification::None};

    /// Free-form impact statement; surfaced to auditors.
    std::string impact_statement;

    /// Optional action statement for status == Fixed.
    std::string action_statement;

    std::chrono::system_clock::time_point timestamp{
        std::chrono::system_clock::now()};
};

struct VexDocument {
    /// Stable identifier. Defaults to a urn:uuid when empty.
    std::string id;

    /// Author identity (RFC 5322 email or organisation name).
    std::string author{"https://euxis.co"};

    /// Author role per OpenVEX §authorRole.
    std::string author_role{"vendor"};

    std::chrono::system_clock::time_point timestamp{
        std::chrono::system_clock::now()};

    std::vector<VexStatement> statements;
};

[[nodiscard]] auto vex_status_str(VexStatus s) noexcept -> const char*;
[[nodiscard]] auto vex_justification_str(VexJustification j) noexcept -> const char*;

/// Convert a VexDocument to OpenVEX JSON.
[[nodiscard]] auto to_openvex(const VexDocument& doc) -> nlohmann::json;

} // namespace euxis::sbom
