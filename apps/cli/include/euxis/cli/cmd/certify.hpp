/// @file
/// @brief Certification readiness assessment command.
#pragma once

#include "euxis/cli/command.hpp"

#include <string>
#include <vector>

namespace euxis::cli::cmd {

/// Certification readiness assessment (full run, controls, report).
int cmd_certify_readiness(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
