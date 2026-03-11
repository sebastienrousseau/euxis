#include "euxis/gateway/routes.hpp"
#include "euxis/gateway/state.hpp"

#include <nlohmann/json.hpp>

namespace euxis::gateway {

void register_health_routes(httplib::Server& server) {
    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json body = {
            {"status", "ok"},
            {"version", "0.0.3"},
            {"timestamp", timestamp()},
        };
        res.set_content(body.dump(), "application/json");
    });

    server.Get("/ready", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json body = {{"ready", true}};
        res.set_content(body.dump(), "application/json");
    });
}

} // namespace euxis::gateway
