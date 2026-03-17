#include "euxis/gateway/config.hpp"

#include <fstream>

namespace euxis::gateway {

auto GatewayConfig::from_json(const nlohmann::json& j) -> GatewayConfig {
    GatewayConfig c;
    c.port = j.value("port", 8080);
    c.ws_port = j.value("ws_port", 8081);
    c.host = j.value("host", "0.0.0.0");
    c.raw = j;
    if (j.contains("timeouts")) {
        auto t = j["timeouts"];
        c.timeouts.webhook = t.value("webhook", 5);
        c.timeouts.health_check = t.value("health_check", 2);
        c.timeouts.api_call = t.value("api_call", 10);
        c.timeouts.tts_command = t.value("tts_command", 30);
    }
    return c;
}

auto GatewayConfig::load_from_file(const std::string& path) -> GatewayConfig {
    std::ifstream f(path);
    if (!f) return {};
    nlohmann::json j;
    f >> j;
    return from_json(j);
}

} // namespace euxis::gateway
