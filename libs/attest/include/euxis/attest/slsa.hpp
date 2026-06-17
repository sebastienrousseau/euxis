/// @file
/// @brief SLSA v1.2 Provenance predicate builder.
///
/// Implements the SLSA Provenance v1.2 schema:
///   https://slsa.dev/spec/v1.2/provenance
///
/// SLSA v1.2 simplified the v1.1 layout: `buildDefinition` (now with
/// resolvedDependencies surfaced as a first-class array) +
/// `runDetails` (builder identity + invocation metadata). euxis emits
/// one Provenance per scan invocation as the `predicate` body of an
/// in-toto Statement.
#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "euxis/attest/statement.hpp"

namespace euxis::attest {

/// Canonical predicateType URI for SLSA v1.2 Provenance.
inline constexpr const char* kSlsaProvenanceV12PredicateType =
    "https://slsa.dev/provenance/v1.2";

/// One entry in resolvedDependencies — links the build to its inputs
/// so verifiers can re-derive the transitive closure offline.
struct ResolvedDependency {
    std::string uri;                                ///< e.g. "git+https://github.com/foo/bar"
    std::unordered_map<std::string, std::string> digest;
    std::string name;                               ///< optional friendly name
};

/// buildDefinition section. `externalParameters` is the user-facing
/// shape (anything a verifier should fingerprint); `internalParameters`
/// is the builder's own ephemera (RNG seed, host OS, etc.).
struct BuildDefinition {
    /// Stable URI identifying the build type. euxis emits a scan
    /// provenance type by default.
    std::string build_type{"https://euxis.co/attest/scan/v1"};

    nlohmann::json external_parameters = nlohmann::json::object();
    nlohmann::json internal_parameters = nlohmann::json::object();
    std::vector<ResolvedDependency> resolved_dependencies;
};

/// Builder identity. SLSA v1.2 requires a stable `id` URI and allows
/// optional `version` and `builderDependencies` maps.
struct Builder {
    std::string id{"https://euxis.co/builder"};
    std::unordered_map<std::string, std::string> version;
    std::vector<ResolvedDependency> builder_dependencies;
};

/// Invocation metadata. RFC 3339 timestamps; `invocation_id` is
/// free-form but should be unique across a single builder.
struct RunMetadata {
    std::string invocation_id;
    std::chrono::system_clock::time_point started_on{
        std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point finished_on{
        std::chrono::system_clock::now()};
};

/// runDetails section.
struct RunDetails {
    Builder builder;
    RunMetadata metadata;
    std::vector<nlohmann::json> byproducts;
};

/// Full SLSA v1.2 Provenance predicate.
struct SlsaProvenance {
    BuildDefinition build_definition;
    RunDetails run_details;
};

/// Serialise to JSON for use as a Statement.predicate.
[[nodiscard]] auto to_json(const SlsaProvenance& p) -> nlohmann::json;

/// Build a Statement from a single subject + SLSA v1.2 provenance.
/// Convenience wrapper for the common case.
[[nodiscard]] auto make_provenance_statement(
    const Subject& subject,
    const SlsaProvenance& provenance) -> Statement;

} // namespace euxis::attest
