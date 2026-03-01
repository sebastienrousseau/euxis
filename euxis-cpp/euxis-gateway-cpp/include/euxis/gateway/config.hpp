#pragma once

#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace euxis::gateway {

struct TimeoutConfig {
    int webhook{5};
    int health_check{2};
    int api_call{10};
    int tts_command{30};
};

struct GatewayConfig {
    int port{8080};
    int ws_port{8081};
    std::string host{"0.0.0.0"};
    TimeoutConfig timeouts;
    nlohmann::json raw;

    static auto from_json(const nlohmann::json& j) -> GatewayConfig;
    static auto load_from_file(const std::string& path) -> GatewayConfig;
};

} // namespace euxis::gateway
