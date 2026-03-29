#include "euxis/gateway/mcp.hpp"
#include "euxis/gateway/state.hpp"
#include "euxis/crypto/ed25519.hpp"

#include <chrono>
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

namespace euxis::gateway {

namespace {

auto hex_encode(std::span<const std::byte> data) -> std::string {
    std::ostringstream oss;
    for (auto b : data) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(b);
    }
    return oss.str();
}

} // namespace

McpHost::McpHost(const nlohmann::json& config) : config_(config) {
    setup_default_tools();
}

void McpHost::register_tool(std::string name, std::string description,
                             nlohmann::json input_schema,
                             McpToolHandler handler) {
    tools_.push_back(
        McpTool{.name = name,
                .description = std::move(description),
                .input_schema = std::move(input_schema)});
    handlers_[std::move(name)] = std::move(handler);
}

void McpHost::register_resource(const std::string& uri,
                                 const std::string& name,
                                 const std::string& description,
                                 const std::string& mime_type,
                                 McpResourceHandler handler) {
    resources_.push_back(McpResource{
        .uri = uri,
        .name = name,
        .description = description,
        .mime_type = mime_type,
    });
    resource_handlers_[uri] = std::move(handler);
}

auto McpHost::handle_request(const nlohmann::json& request) -> nlohmann::json {
    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0" ||
        !request.contains("method")) {
        return make_error(request.value("id", nlohmann::json(nullptr)),
                          -32600, "Invalid Request");
    }

    const auto& method = request["method"].get<std::string>();
    auto id = request.value("id", nlohmann::json(nullptr));

    // initialize is always allowed
    if (method == "initialize") {
        return handle_initialize(request);
    }

    // notifications/initialized is a no-op ack
    if (method == "notifications/initialized") {
        return make_result(id, nlohmann::json::object());
    }

    // Guard: reject non-initialize methods before initialization
    if (!initialized_) {
        return make_error(id, -32002, "Server not initialized");
    }

    if (method == "tools/list") {
        return handle_tools_list(request);
    }
    if (method == "tools/call") {
        return handle_tools_call(request);
    }
    if (method == "resources/list") {
        return handle_resources_list(request);
    }
    if (method == "resources/read") {
        return handle_resources_read(request);
    }
    if (method == "ping") {
        return handle_ping(request);
    }

    return make_error(id, -32601, "Method not found");
}

auto McpHost::tools() const -> const std::vector<McpTool>& {
    return tools_;
}

auto McpHost::resources() const -> const std::vector<McpResource>& {
    return resources_;
}

auto McpHost::handle_initialize(const nlohmann::json& req) -> nlohmann::json {
    initialized_ = true;
    nlohmann::json result = {
        {"protocolVersion", protocol_version_},
        {"capabilities", {{"tools", {{"listChanged", true}}},
                          {"resources", {{"listChanged", true}}}}},
        {"serverInfo", {{"name", "euxis-gateway"}, {"version", "0.0.3"}}},
    };
    return make_result(req.value("id", nlohmann::json(nullptr)), result);
}

auto McpHost::handle_tools_list(const nlohmann::json& req) -> nlohmann::json {
    auto tools_json = nlohmann::json::array();
    for (const auto& tool : tools_) {
        tools_json.push_back({
            {"name", tool.name},
            {"description", tool.description},
            {"inputSchema", tool.input_schema},
        });
    }
    return make_result(req.value("id", nlohmann::json(nullptr)),
                       {{"tools", tools_json}});
}

auto McpHost::handle_tools_call(const nlohmann::json& req) -> nlohmann::json {
    auto id = req.value("id", nlohmann::json(nullptr));
    auto params = req.value("params", nlohmann::json::object());
    auto tool_name = params.value("name", std::string{});

    auto it = handlers_.find(tool_name);
    if (it == handlers_.end()) {
        return make_error(id, -32601,
                          "Unknown tool: " + tool_name);
    }

    try {
        auto args = params.value("arguments", nlohmann::json::object());
        auto result = it->second(args);
        nlohmann::json content = {{{"type", "text"},
                                   {"text", result.dump()}}};
        return make_result(id, {{"content", content}});
    } catch (const std::exception& e) {
        return make_error(id, -32603, std::string("Tool error: ") + e.what());
    }
}

auto McpHost::handle_resources_list(const nlohmann::json& req)
    -> nlohmann::json {
    auto resources_json = nlohmann::json::array();
    for (const auto& r : resources_) {
        resources_json.push_back({
            {"uri", r.uri},
            {"name", r.name},
            {"description", r.description},
            {"mimeType", r.mime_type},
        });
    }
    return make_result(req.value("id", nlohmann::json(nullptr)),
                       {{"resources", resources_json}});
}

auto McpHost::handle_resources_read(const nlohmann::json& req)
    -> nlohmann::json {
    auto id = req.value("id", nlohmann::json(nullptr));
    auto params = req.value("params", nlohmann::json::object());
    auto uri = params.value("uri", std::string{});

    auto it = resource_handlers_.find(uri);
    if (it == resource_handlers_.end()) {
        return make_error(id, -32602, "Unknown resource URI: " + uri);
    }

    try {
        auto content = it->second(uri);
        return make_result(id, {{"contents", {{
            {"uri", uri},
            {"mimeType", "application/json"},
            {"text", content.dump()},
        }}}});
    } catch (const std::exception& e) {
        return make_error(id, -32603,
                          std::string("Resource error: ") + e.what());
    }
}

auto McpHost::handle_ping(const nlohmann::json& req) -> nlohmann::json {
    return make_result(req.value("id", nlohmann::json(nullptr)),
                       nlohmann::json::object());
}

auto McpHost::make_result(const nlohmann::json& id,
                           const nlohmann::json& result) -> nlohmann::json {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

auto McpHost::make_error(const nlohmann::json& id, int code,
                          const std::string& msg) -> nlohmann::json {
    return {{"jsonrpc", "2.0"},
            {"id", id},
            {"error", {{"code", code}, {"message", msg}}}};
}

void McpHost::setup_default_tools() {
    register_tool(
        "get_metrics", "Return current gateway metrics",
        {{"type", "object"}, {"properties", nlohmann::json::object()}},
        [](const nlohmann::json& /*args*/) -> nlohmann::json {
            auto now = std::chrono::steady_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now.time_since_epoch())
                              .count();
            return {{"uptime_ms", uptime},
                    {"sessions_active", 0},
                    {"timestamp", timestamp()}};
        });

    register_tool(
        "sign_payload", "Sign a payload with Ed25519",
        {{"type", "object"},
         {"properties",
          {{"payload", {{"type", "string"}, {"description", "Data to sign"}}}}},
         {"required", {"payload"}}},
        [](const nlohmann::json& args) -> nlohmann::json {
            auto payload = args.value("payload", std::string{});
            auto kp = euxis::crypto::generate_keypair();
            std::vector<std::byte> msg(payload.size());
            std::memcpy(msg.data(), payload.data(), payload.size());
            auto sig = euxis::crypto::sign(
                std::span<const std::byte, 64>(kp.secret_key), msg);
            return {{"payload", payload},
                    {"signature", hex_encode(sig)},
                    {"algorithm", "ed25519"}};
        });
}

} // namespace euxis::gateway
