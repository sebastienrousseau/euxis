/// @file
/// @brief Dev commands
#pragma once

#include "euxis/cli/command.hpp"

#include <vector>
#include <string>

namespace euxis::cli::cmd {

int cmd_bench(Context& ctx, const std::vector<std::string>& args);
int cmd_hooks(Context& ctx, const std::vector<std::string>& args);
int cmd_setup_shell_hooks(Context& ctx, const std::vector<std::string>& args);
int cmd_git_guard(Context& ctx, const std::vector<std::string>& args);
int cmd_license_check(Context& ctx, const std::vector<std::string>& args);
int cmd_docs_test(Context& ctx, const std::vector<std::string>& args);
int cmd_sync_docs(Context& ctx, const std::vector<std::string>& args);
int cmd_test_infra(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
