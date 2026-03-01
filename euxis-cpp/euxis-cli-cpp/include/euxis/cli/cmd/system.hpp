#pragma once

#include "euxis/cli/command.hpp"

#include <vector>
#include <string>

namespace euxis::cli::cmd {

int cmd_doctor(Context& ctx, const std::vector<std::string>& args);
int cmd_health(Context& ctx, const std::vector<std::string>& args);
int cmd_verify(Context& ctx, const std::vector<std::string>& args);
int cmd_lint(Context& ctx, const std::vector<std::string>& args);
int cmd_shell_lint(Context& ctx, const std::vector<std::string>& args);
int cmd_verify_all(Context& ctx, const std::vector<std::string>& args);
int cmd_cross_platform_verify(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
