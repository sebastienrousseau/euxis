/// @file
/// @brief Knowledge commands
#pragma once

#include "euxis/cli/command.hpp"

#include <vector>
#include <string>

namespace euxis::cli::cmd {

int cmd_cortex(Context& ctx, const std::vector<std::string>& args);
int cmd_graph(Context& ctx, const std::vector<std::string>& args);
int cmd_codex(Context& ctx, const std::vector<std::string>& args);
int cmd_omnigraph(Context& ctx, const std::vector<std::string>& args);
int cmd_slash(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
