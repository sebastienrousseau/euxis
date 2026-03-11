#include <euxis/network/mcp_client.hpp>
#include <euxis/a2a/transport.hpp>
#include <euxis/a2a/message.hpp>
#include <spdlog/spdlog.h>

namespace euxis::network {

McpClient::McpClient(std::shared_ptr<euxis::a2a::ITransport> transport)
    : transport_(std::move(transport)) {}

auto McpClient::send_rpc(const std::string& method, const nlohmann::json& params)
    -> std::expected<nlohmann::json, std::string> {
    
    nlohmann::json rpc_req = {
        {"jsonrpc", "2.0"},
        {"id", ++request_id_},
        {"method", method}
    };
    if (params != nullptr) {
        rpc_req["params"] = params;
    }

    // Convert JSON-RPC to A2A Message format
    euxis::a2a::Message msg;
    msg.payload = rpc_req.dump();
    msg.id = std::to_string(request_id_);

    auto response = transport_->send(msg);
    if (!response) {
        return std::unexpected("Transport error during MCP call to " + method);
    }

    try {
        auto res_json = nlohmann::json::parse(response->payload);
        if (res_json.contains("error")) {
            return std::unexpected("MCP Server Error: " + res_json["error"].dump());
        }
        return res_json["result"];
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Failed to parse MCP response: ") + e.what());
    }
}

auto McpClient::initialize() -> std::expected<nlohmann::json, std::string> {
    nlohmann::json params = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {}},
        {"clientInfo", {{"name", "euxis-mcp-client"}, {"version", "0.1.0"}}}
    };
    
    auto res = send_rpc("initialize", params);
    if (res) {
        // Send initialized notification as per protocol
        auto _ = send_rpc("notifications/initialized");
    }
    return res;
}

auto McpClient::list_tools() -> std::expected<nlohmann::json, std::string> {
    return send_rpc("tools/list");
}

auto McpClient::call_tool(const std::string& name, const nlohmann::json& arguments) 
    -> std::expected<nlohmann::json, std::string> {
    nlohmann::json params = {
        {"name", name},
        {"arguments", arguments}
    };
    return send_rpc("tools/call", params);
}

} // namespace euxis::network
