/// @file
/// @brief Infra commands
#pragma once

#include "euxis/cli/command.hpp"

#include <vector>
#include <string>

namespace euxis::cli::cmd {

int cmd_gateway(Context& ctx, const std::vector<std::string>& args);
int cmd_bus(Context& ctx, const std::vector<std::string>& args);
int cmd_daemon(Context& ctx, const std::vector<std::string>& args);
int cmd_deploy(Context& ctx, const std::vector<std::string>& args);
int cmd_optimize(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
