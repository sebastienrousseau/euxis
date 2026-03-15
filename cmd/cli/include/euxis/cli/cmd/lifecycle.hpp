/// @file
/// @brief Lifecycle management commands — install, update, upgrade, uninstall, self.
#pragma once

#include "euxis/cli/command.hpp"

#include <string>
#include <vector>

namespace euxis::cli::cmd {

/// Bootstrap local Euxis installation.
int cmd_install(Context& ctx, const std::vector<std::string>& args);

/// Refresh metadata and registry.
int cmd_update(Context& ctx, const std::vector<std::string>& args);

/// Upgrade binary (pull + rebuild).
int cmd_upgrade(Context& ctx, const std::vector<std::string>& args);

/// Remove Euxis from this machine.
int cmd_uninstall(Context& ctx, const std::vector<std::string>& args);

/// Installation introspection (status, paths, doctor, version, where, repair).
int cmd_self(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
