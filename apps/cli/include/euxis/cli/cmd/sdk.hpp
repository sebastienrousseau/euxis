/// @file
/// @brief SDK demonstration commands — proves the libs/runtime harness +
///        libs/metrics insights pipeline is wired end-to-end.
#pragma once

#include "euxis/cli/command.hpp"

#include <string>
#include <vector>

namespace euxis::cli::cmd {

/// @brief `euxis sdk-demo` — drive AgentLoopHarness through N mock turns
///        and emit per-session insights.
int cmd_sdk_demo(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
