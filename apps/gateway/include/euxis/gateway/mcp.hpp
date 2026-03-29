/// @file
/// @brief Gateway MCP
#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::gateway {

struct McpTool {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
};

using McpToolHandler = std::function<nlohmann::json(const nlohmann::json& args)>;

struct McpResource {
    std::string uri;
    std::string name;
    std::string description;
    std::string mime_type;
};

using McpResourceHandler = std::function<nlohmann::json(const std::string& uri)>;

class McpHost {
public:
    explicit McpHost(const nlohmann::json& config = {});

    void register_tool(std::string name, std::string description,
                       nlohmann::json input_schema, McpToolHandler handler);

    void register_resource(const std::string& uri, const std::string& name,
                           const std::string& description,
                           const std::string& mime_type,
                           McpResourceHandler handler);

    auto handle_request(const nlohmann::json& request) -> nlohmann::json;
    [[nodiscard]] auto tools() const -> const std::vector<McpTool>&;
    [[nodiscard]] auto resources() const -> const std::vector<McpResource>&;

    static auto make_result(const nlohmann::json& id,
                            const nlohmann::json& result) -> nlohmann::json;
    static auto make_error(const nlohmann::json& id, int code,
                           const std::string& msg) -> nlohmann::json;

private:
    std::string protocol_version_{"2024-11-05"};
    bool initialized_{false};
    std::vector<McpTool> tools_;
    std::unordered_map<std::string, McpToolHandler> handlers_;
    std::vector<McpResource> resources_;
    std::unordered_map<std::string, McpResourceHandler> resource_handlers_;
    nlohmann::json config_;

    auto handle_initialize(const nlohmann::json& req) -> nlohmann::json;
    auto handle_tools_list(const nlohmann::json& req) -> nlohmann::json;
    auto handle_tools_call(const nlohmann::json& req) -> nlohmann::json;
    auto handle_resources_list(const nlohmann::json& req) -> nlohmann::json;
    auto handle_resources_read(const nlohmann::json& req) -> nlohmann::json;
    auto handle_ping(const nlohmann::json& req) -> nlohmann::json;

    void setup_default_tools();
};

} // namespace euxis::gateway
