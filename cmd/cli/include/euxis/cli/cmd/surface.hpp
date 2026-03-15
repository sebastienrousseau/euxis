/// @file
/// @brief Core surface commands — user-facing wrappers around the playbook engine.
#pragma once

#include "euxis/cli/command.hpp"

#include <string>
#include <vector>

namespace euxis::cli::cmd {

/// Verify a repository or target (default: standard mode).
int cmd_check(Context& ctx, const std::vector<std::string>& args);

/// Fast bounded triage scan (flash mode).
int cmd_triage(Context& ctx, const std::vector<std::string>& args);

/// Deep verification (standard or forensic mode).
int cmd_review(Context& ctx, const std::vector<std::string>& args);

/// Compare triage vs deep verification on the same target.
int cmd_compare(Context& ctx, const std::vector<std::string>& args);

/// Validation metrics and drift history.
int cmd_stats(Context& ctx, const std::vector<std::string>& args);

/// Policy inspection and enforcement.
int cmd_policy(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
