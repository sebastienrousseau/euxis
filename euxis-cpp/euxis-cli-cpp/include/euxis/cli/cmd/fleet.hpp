/// @file
/// @brief Fleet commands
#pragma once

#include "euxis/cli/command.hpp"

#include <vector>
#include <string>

namespace euxis::cli::cmd {

int cmd_agent(Context& ctx, const std::vector<std::string>& args);
int cmd_agent_bootstrap(Context& ctx, const std::vector<std::string>& args);
int cmd_squad(Context& ctx, const std::vector<std::string>& args);
int cmd_combo(Context& ctx, const std::vector<std::string>& args);
int cmd_playbook(Context& ctx, const std::vector<std::string>& args);
int cmd_dispatch(Context& ctx, const std::vector<std::string>& args);
int cmd_council(Context& ctx, const std::vector<std::string>& args);
int cmd_loop(Context& ctx, const std::vector<std::string>& args);
int cmd_synthesize(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
