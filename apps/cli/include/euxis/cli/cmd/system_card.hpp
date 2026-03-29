#pragma once

#include "euxis/cli/cmd/fleet.hpp"

namespace euxis::cli::cmd {

/// Generate a system card for the euxis CLI.
int cmd_system_card(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
