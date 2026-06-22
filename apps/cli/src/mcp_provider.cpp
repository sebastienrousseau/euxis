#include "euxis/cli/mcp_provider.hpp"
#include "euxis/cli/process.hpp"

#include <fstream>
#include <filesystem>
#include <sstream>

namespace euxis::cli {

McpProviderBridge::McpProviderBridge(const std::string& data_dir) {
    load_config(data_dir);
}

void McpProviderBridge::load_config(const std::string& data_dir) {
    auto config_path = std::filesystem::path(data_dir) / "config" / "router.json";
    if (!std::filesystem::exists(config_path)) return;

    try {
        std::ifstream f(config_path);
        auto config = nlohmann::json::parse(f);

        if (!config.contains("mcp_servers") || !config["mcp_servers"].is_object()) return;

        for (auto& [name, server] : config["mcp_servers"].items()) {
            McpServerConfig cfg;
            cfg.name = name;
            cfg.command = server.value("command", "");
            if (server.contains("args") && server["args"].is_array()) {
                for (const auto& arg : server["args"]) {
                    cfg.args.push_back(arg.get<std::string>());
                }
            }
            if (server.contains("env") && server["env"].is_array()) {
                for (const auto& e : server["env"]) {
                    cfg.env.push_back(e.get<std::string>());
                }
            }
            cfg.timeout_seconds = server.value("timeout", 30);
            servers_[name] = std::move(cfg);
        }
    } catch (...) {
        // Config parse failure — no servers available
        (void)0;  // swallowed: best-effort
    }
}

auto McpProviderBridge::execute(const std::string& server_name,
                                 const std::string& tool_name,
                                 const nlohmann::json& arguments,
                                 int timeout_seconds) -> ProviderResponse {
    auto it = servers_.find(server_name);
    if (it == servers_.end()) {
        return {false, "", "unknown MCP server: " + server_name, 1, 0.0, {}};
    }

    const auto& cfg = it->second;
    if (cfg.command.empty()) {
        return {false, "", "MCP server has no command: " + server_name, 1, 0.0, {}};
    }

    // Build JSON-RPC call_tool request
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/call"},
        {"params", {
            {"name", tool_name},
            {"arguments", arguments}
        }}
    };

    // Frame with Content-Length
    std::string body = request.dump();
    std::string framed = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

    // Add initialize handshake before the call
    nlohmann::json init_req = {
        {"jsonrpc", "2.0"}, {"id", 0}, {"method", "initialize"},
        {"params", {{"protocolVersion", "2024-11-05"}, {"capabilities", {}},
                     {"clientInfo", {{"name", "euxis"}, {"version", "0.1.3"}}}}}
    };
    std::string init_body = init_req.dump();
    std::string init_framed = "Content-Length: " + std::to_string(init_body.size()) + "\r\n\r\n" + init_body;

    nlohmann::json init_ack = {{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}};
    std::string ack_body = init_ack.dump();
    std::string ack_framed = "Content-Length: " + std::to_string(ack_body.size()) + "\r\n\r\n" + ack_body;

    std::string full_input = init_framed + ack_framed + framed;

    // Q2: Set env vars with allowlist validation — reject shell metacharacters
    static const std::string_view allowed_key_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
    std::map<std::string, std::string> env_map;
    for (const auto& e : cfg.env) {
        auto eq = e.find('=');
        if (eq == std::string::npos) continue;
        auto key = e.substr(0, eq);
        auto val = e.substr(eq + 1);
        // Reject keys with non-identifier characters
        if (key.find_first_not_of(allowed_key_chars) != std::string::npos) continue;
        // Reject values with shell metacharacters
        bool safe = true;
        for (char c : val) {
            if (c == '`' || c == '$' || c == ';' || c == '|' || c == '\n' || c == '\r' || c == '\0') {
                safe = false;
                break;
            }
        }
        if (safe) env_map[key] = val;
    }

    auto result = Process::run_with_input(cfg.command, cfg.args, full_input,
                                           timeout_seconds > 0 ? timeout_seconds : cfg.timeout_seconds,
                                           env_map);

    if (result.exit_code != 0 && result.stdout_output.empty()) {
        return {false, "", "MCP server failed: " + result.stderr_output, result.exit_code, 0.0, {}};
    }

    // Parse Content-Length framed responses — take the last one (the tool call response)
    std::string output = result.stdout_output;
    nlohmann::json last_response;
    size_t pos = 0;
    while (pos < output.size()) {
        auto header_end = output.find("\r\n\r\n", pos);
        if (header_end == std::string::npos) break;

        // Parse content length
        auto cl_pos = output.find("Content-Length: ", pos);
        if (cl_pos == std::string::npos || cl_pos > header_end) break;

        size_t cl_val = 0;
        try { cl_val = std::stoull(output.substr(cl_pos + 16)); } catch (...) { break; }

        size_t body_start = header_end + 4;
        if (body_start + cl_val > output.size()) break;

        std::string response_body = output.substr(body_start, cl_val);
        try { last_response = nlohmann::json::parse(response_body); } catch (...) { /* swallowed: best-effort path */ (void)0; }

        pos = body_start + cl_val;
    }

    if (last_response.is_null()) {
        return {false, output, "failed to parse MCP response", 1, 0.0, {}};
    }

    if (last_response.contains("error")) {
        return {false, "", "MCP error: " + last_response["error"].dump(), 1, 0.0, {}};
    }

    if (last_response.contains("result")) {
        std::string content;
        auto& r = last_response["result"];
        if (r.contains("content") && r["content"].is_array()) {
            for (const auto& c : r["content"]) {
                if (c.contains("text")) content += c["text"].get<std::string>();
            }
        } else {
            content = r.dump();
        }
        return {true, content, "", 0, 0.0, {}};
    }

    return {true, last_response.dump(), "", 0, 0.0, {}};
}

auto McpProviderBridge::has_server(const std::string& name) const -> bool {
    return servers_.count(name) > 0;
}

auto McpProviderBridge::server_names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    for (const auto& [k, v] : servers_) names.push_back(k);
    return names;
}

} // namespace euxis::cli
