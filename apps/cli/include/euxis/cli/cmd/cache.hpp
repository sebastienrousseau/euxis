/// @file
/// @brief `euxis cache` — inspect, purge, and prune the scan cache.
#pragma once

#include "euxis/cli/command.hpp"

#include <string>
#include <vector>

namespace euxis::cli::cmd {

/// `euxis cache <subcommand> [--db=<path>]`
///
/// Subcommands:
///   stats          (default) — print entry count, size, age of
///                              oldest and newest entry
///   purge          remove every cached entry
///   purge-older   --ttl-days=<n>  remove entries older than n days
///   path           print the resolved cache database path
///
/// The default cache database lives at
/// `${XDG_CACHE_HOME-$HOME/.cache}/euxis/scan-cache.sqlite`. Override
/// per-invocation with `--db=<path>` or persistently via
/// `.euxis.yaml: cache.path`.
int cmd_cache(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
