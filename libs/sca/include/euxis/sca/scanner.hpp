/// @file
/// @brief Directory-walking scanner that dispatches to ecosystem parsers.
///
/// Walks the target tree, dispatches every recognised lockfile to its
/// parser, merges the resulting `ParsedManifest` instances, and turns
/// them into a single `euxis::sbom::SbomDocument` that the CycloneDX/
/// SPDX/OpenVEX emitters can consume.
#pragma once

#include <expected>
#include <filesystem>
#include <vector>

#include "euxis/sbom/types.hpp"
#include "euxis/sca/manifest.hpp"

namespace euxis::sca {

struct ScanOptions {
    /// Default ignore patterns. Tests can override; production
    /// callers should override via `.euxis.yaml` (P0.4c).
    std::vector<std::string> exclude{
        "node_modules",
        "vendor",
        ".venv",
        ".git",
        "target",
        "build",
        "cmake-build",
        "dist",
    };

    /// Cap on the number of lockfiles to consider in a single scan.
    /// Default protects against pathological monorepos producing
    /// 50K+ lockfile parses.
    std::size_t max_files{4096};
};

/// Result of scanning a directory tree.
struct ScanResult {
    std::vector<ParsedManifest> manifests;
    std::vector<ParseError>     errors;     ///< Non-fatal parse errors
};

/// Walk `root`, locate lockfiles, parse them. Errors on a per-file
/// basis are surfaced in `ScanResult::errors`; a fatal error (e.g.
/// `root` does not exist) is returned in the unexpected branch.
[[nodiscard]] auto scan_directory(const std::filesystem::path& root,
                                  const ScanOptions& opts = {})
    -> std::expected<ScanResult, ParseError>;

/// Convert a ScanResult to a euxis::sbom::SbomDocument with one
/// component per (purl) entry. Duplicates across manifests are
/// deduplicated by `(purl)`; the dep graph is constructed by
/// resolving dependency names within each ecosystem.
[[nodiscard]] auto to_sbom_document(const ScanResult& scan,
                                    const std::filesystem::path& project_root)
    -> euxis::sbom::SbomDocument;

} // namespace euxis::sca
