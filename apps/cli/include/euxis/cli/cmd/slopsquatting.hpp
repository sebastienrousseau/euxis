/// @file
/// @brief `euxis slopsquatting` — guard against LLM-hallucinated
///        package names in dependency manifests.
#pragma once

#include "euxis/cli/command.hpp"

#include <string>
#include <vector>

namespace euxis::cli::cmd {

/// `euxis slopsquatting [target]
///                      [--corpus=<path>] [--no-seed]
///                      [--sarif=<path>] [--quiet]
///                      [--staged]`
///
/// Walks the target directory (default `.`), parses every recognised
/// lockfile via libs/sca, and checks each declared dependency name
/// against the slopsquatting corpus from libs/slopsquatting. The
/// embedded Spracklen-style seed is loaded by default; pass
/// `--corpus=<path>` to layer the full 205K-name corpus on top.
///
/// `--staged` is a pre-commit-hook convenience: scans only the
/// lockfiles among staged changes (`git diff --staged --name-only`).
///
/// Exit codes:
///   0 no matches found
///   1 invalid arguments or scan failure
///   3 at least one finding emitted (CI gate)
int cmd_slopsquatting(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
