#include "euxis/gateway/routes.hpp"
#include "euxis/gateway/state.hpp"

#include <nlohmann/json.hpp>

namespace euxis::gateway {

// NOLINTBEGIN(bugprone-exception-escape) — see routes_admin.cpp for
// the cpp-httplib server-level catch rationale.
void register_webhook_routes(httplib::Server& server, const RouteContext& ctx) {
    server.Post("/api/webhooks/inbound",
                [ctx](const httplib::Request& req, httplib::Response& res) {
                    if (!authorize_request(req, res, ctx)) return;
                    try {
                        auto j = nlohmann::json::parse(req.body);
                        audit_log({{"event", "webhook.inbound"},
                                   {"payload", j},
                                   {"timestamp", timestamp()}});
                        res.set_content(R"({"ok":true})", "application/json");
                    } catch (const std::exception&) {
                        res.status = 400;
                        res.set_content(R"({"error":"invalid json"})",
                                        "application/json");
                    }
                });
}
// NOLINTEND(bugprone-exception-escape)

} // namespace euxis::gateway
