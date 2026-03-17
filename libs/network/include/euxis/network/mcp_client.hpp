#pragma once

#include <expected>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::a2a {
class ITransport; // Forward declaration
class Message;
}

namespace euxis::network {

/**
 * @brief MCP Client for consuming external 2026 tools via A2A transport.
 */
class McpClient {
public:
    explicit McpClient(std::shared_ptr<euxis::a2a::ITransport> transport);
    ~McpClient() = default;

    /**
     * @brief Initialize connection with the MCP server.
     */
    auto initialize() -> std::expected<nlohmann::json, std::string>;

    /**
     * @brief Discover available tools on the external MCP server.
     */
    auto list_tools() -> std::expected<nlohmann::json, std::string>;

    /**
     * @brief Execute a remote tool via the MCP protocol.
     */
    auto call_tool(const std::string& name, const nlohmann::json& arguments) 
        -> std::expected<nlohmann::json, std::string>;

private:
    auto send_rpc(const std::string& method, const nlohmann::json& params = nullptr) 
        -> std::expected<nlohmann::json, std::string>;

    std::shared_ptr<euxis::a2a::ITransport> transport_;
    int request_id_{0};
};

} // namespace euxis::network
