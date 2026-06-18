/// @file
/// @brief `euxis vulndb` subcommands — direct OSV.dev lookups and
///        (future) offline-dump synchronisation.

#pragma once

#include <string>
#include <vector>

#include "euxis/cli/command.hpp"

namespace euxis::cli::cmd {

/// `euxis vulndb <subcommand> [args]`
/// Subcommands:
///   query <PURL> [--json]   — single-PURL OSV.dev lookup
///   sync                    — placeholder for offline-dump fetch
auto cmd_vulndb(Context& ctx, const std::vector<std::string>& args) -> int;

} // namespace euxis::cli::cmd
