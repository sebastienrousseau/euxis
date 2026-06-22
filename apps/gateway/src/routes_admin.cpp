#include "euxis/gateway/routes.hpp"
#include "euxis/gateway/state.hpp"

#include <nlohmann/json.hpp>

namespace euxis::gateway {

// NOLINTBEGIN(bugprone-exception-escape) — cpp-httplib wraps every
// handler call in its own try/catch and returns 500 on uncaught
// throws (see _deps/cpp-httplib's `process_request` path), so a
// handler exception cannot terminate the gateway. clang-tidy
// doesn't model that catch site.
void register_admin_routes(httplib::Server& server, const RouteContext& ctx) {
    server.Get("/api/admin/config",
               [ctx](const httplib::Request& req, httplib::Response& res) {
                   if (!authorize_request(req, res, ctx)) return;
                   nlohmann::json body = {
                       {"version", "0.0.3"},
                       {"runtime", "cpp"},
                       {"timestamp", timestamp()},
                   };
                   res.set_content(body.dump(), "application/json");
               });
}
// NOLINTEND(bugprone-exception-escape)

} // namespace euxis::gateway
