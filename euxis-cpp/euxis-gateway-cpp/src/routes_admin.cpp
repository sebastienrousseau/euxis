#include "euxis/gateway/routes.hpp"
#include "euxis/gateway/state.hpp"

#include <nlohmann/json.hpp>

namespace euxis::gateway {

void register_admin_routes(httplib::Server& server) {
    server.Get("/api/admin/config",
               [](const httplib::Request&, httplib::Response& res) {
                   nlohmann::json body = {
                       {"version", "0.0.3"},
                       {"runtime", "cpp"},
                       {"timestamp", timestamp()},
                   };
                   res.set_content(body.dump(), "application/json");
               });
}

} // namespace euxis::gateway
