/// @file
/// @brief `euxis sbom` — emit a CycloneDX 1.6, SPDX 3.0.1, or OpenVEX
///        document from a directory scan.
#pragma once

#include "euxis/cli/command.hpp"

#include <string>
#include <vector>

namespace euxis::cli::cmd {

/// `euxis sbom <target> [--format=cyclonedx|spdx|both|openvex]
///                      [--output=<path>] [--pretty]`
///
/// Walks the target directory, dispatches every recognised lockfile
/// (Cargo.lock, package-lock.json, Pipfile.lock, go.sum) to its
/// parser via libs/sca, builds a canonical SbomDocument, then emits
/// the requested format(s).
///
/// Exit codes match docs/reference/exit-codes.md:
///   0  scan completed, output written
///   1  invalid arguments or parse failure
///
/// `openvex` emits an empty-statement VEX document seeded with the
/// document author and timestamp; statements are appended by the
/// later reachability stage (P1.7).
int cmd_sbom(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
