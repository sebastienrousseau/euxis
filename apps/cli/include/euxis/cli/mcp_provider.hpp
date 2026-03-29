#pragma once

#include "euxis/cli/provider_executor.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <optional>

namespace euxis::cli {

/// Configuration for an external MCP server.
struct McpServerConfig {
    std::string name;
    std::string command;      // Command to launch the server
    std::vector<std::string> args;
    std::vector<std::string> env;
    int timeout_seconds{30};
};

/// Bridge between MCP servers and the euxis provider executor.
class McpProviderBridge {
public:
    /// Load MCP server configurations from router.json.
    explicit McpProviderBridge(const std::string& data_dir);

    /// Execute a tool on a named MCP server.
    [[nodiscard]] auto execute(const std::string& server_name,
                                const std::string& tool_name,
                                const nlohmann::json& arguments,
                                int timeout_seconds = 30) -> ProviderResponse;

    /// Check if a server is configured.
    [[nodiscard]] auto has_server(const std::string& name) const -> bool;

    /// Get list of configured server names.
    [[nodiscard]] auto server_names() const -> std::vector<std::string>;

private:
    std::unordered_map<std::string, McpServerConfig> servers_;
    void load_config(const std::string& data_dir);
};

} // namespace euxis::cli
