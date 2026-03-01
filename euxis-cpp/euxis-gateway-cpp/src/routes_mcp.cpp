#include "euxis/gateway/routes.hpp"
#include "euxis/gateway/mcp.hpp"

#include <spdlog/spdlog.h>

namespace euxis::gateway {

void register_mcp_routes(httplib::Server& server) {
    server.Post("/mcp", [](const httplib::Request& req, httplib::Response& res) {
        static McpHost host;
        try {
            auto request = nlohmann::json::parse(req.body);
            auto response = host.handle_request(request);
            res.set_content(response.dump(), "application/json");
        } catch (const nlohmann::json::parse_error&) {
            auto err = McpHost::make_error(nullptr, -32700, "Parse error");
            res.set_content(err.dump(), "application/json");
        }
    });
}

} // namespace euxis::gateway
