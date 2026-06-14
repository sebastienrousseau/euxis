/// @file
/// @brief `euxis scan` — apply OpenGrep-format YAML rule packs to a
///        target directory and emit findings as SARIF.
#pragma once

#include "euxis/cli/command.hpp"

#include <string>
#include <vector>

namespace euxis::cli::cmd {

/// `euxis scan [target]
///              --rules=<path>
///              [--sarif=<path>] [--no-sarif]
///              [--language=<lang>]
///              [--fail-on=<sev>]
///              [--max-files=<n>]
///              [--quiet]`
///
/// Walks `target` (default: cwd), parses every supported file via
/// libs/parse, projects each AST to a CPG via libs/cpg::build, then
/// applies the rule pack(s) from --rules via libs/scan.
///
/// Output: SARIF 2.1.0 with CWE/OWASP taxa to `--sarif=<path>`
/// (default `euxis-scan.sarif.json`). A summary line is printed
/// unless --quiet.
///
/// Exit codes (per docs/reference/exit-codes.md):
///   0  no findings at or above --fail-on
///   1  infra error (rules unreadable, scan target missing, ...)
///   2  advisory findings (below --fail-on, default Medium)
///   3  blocking findings (at or above --fail-on, default High)
int cmd_scan(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
