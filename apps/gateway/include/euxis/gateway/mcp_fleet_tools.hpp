#pragma once

#include "euxis/gateway/mcp.hpp"

#include <string>

namespace euxis::gateway {

/// Register fleet tools on an MCP host.
void register_fleet_tools(McpHost& host, const std::string& euxis_home);

} // namespace euxis::gateway
