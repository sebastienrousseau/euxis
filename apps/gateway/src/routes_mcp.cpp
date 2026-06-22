#include "euxis/gateway/routes.hpp"
#include "euxis/gateway/mcp.hpp"

#include <spdlog/spdlog.h>

namespace euxis::gateway {

// NOLINTBEGIN(bugprone-exception-escape) — see routes_admin.cpp for
// the cpp-httplib server-level catch rationale.
void register_mcp_routes(httplib::Server& server, const RouteContext& ctx) {
    server.Post("/mcp", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!authorize_request(req, res, ctx)) return;
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
// NOLINTEND(bugprone-exception-escape)

} // namespace euxis::gateway
