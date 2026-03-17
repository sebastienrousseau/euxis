/// @file
/// @brief Specialized commands
#pragma once

#include "euxis/cli/command.hpp"

#include <vector>
#include <string>

#include <iostream>

namespace euxis::cli::cmd {

int cmd_voice(Context& ctx, const std::vector<std::string>& args);
int cmd_voice_ex(Context& ctx, const std::vector<std::string>& args, std::istream& input = std::cin);
int cmd_tui(Context& ctx, const std::vector<std::string>& args);
int cmd_tui_ex(Context& ctx, const std::vector<std::string>& args, std::istream& input = std::cin);
int cmd_gui(Context& ctx, const std::vector<std::string>& args);
int cmd_polish(Context& ctx, const std::vector<std::string>& args);
int cmd_kaizen(Context& ctx, const std::vector<std::string>& args);
int cmd_audit(Context& ctx, const std::vector<std::string>& args);
int cmd_audit_run(Context& ctx, const std::vector<std::string>& args);
int cmd_certify(Context& ctx, const std::vector<std::string>& args);
int cmd_evidence_verify(Context& ctx, const std::vector<std::string>& args);
int cmd_gym(Context& ctx, const std::vector<std::string>& args);
int cmd_replay(Context& ctx, const std::vector<std::string>& args);
int cmd_context_worker(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
