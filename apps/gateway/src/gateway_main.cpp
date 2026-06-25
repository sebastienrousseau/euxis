#include "euxis/gateway/main_entry.hpp"

#include "euxis/gateway/config.hpp"
#include "euxis/gateway/server.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace euxis::gateway {

int gateway_main(int argc, char* argv[], bool start_server) {
    std::string config_path;

    if (argc > 1) {
        config_path = argv[1];
    } else {
        const char* home = std::getenv("EUXIS_HOME");
        if (!home) {
            const char* user_home = std::getenv("HOME");
            if (!user_home) {
                std::cerr << "error: Neither EUXIS_HOME nor HOME is set\n";
                return 1;
            }
            config_path = (std::filesystem::path(user_home) / ".euxis" / "data" / "config" / "gateway-e2e.json").string();
        } else {
            config_path = (std::filesystem::path(home) / "data" / "config" / "gateway-e2e.json").string();
        }
    }

    if (!std::filesystem::exists(config_path)) {
        std::cerr << "error: config not found: " << config_path << "\n";
        return 1;
    }

    auto config = GatewayConfig::load_from_file(config_path);
    if (!start_server) {
        return 0;
    }
    GatewayServer server(std::move(config));
    server.start();

    return 0;
}

} // namespace euxis::gateway
