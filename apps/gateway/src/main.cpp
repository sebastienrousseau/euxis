#include "euxis/gateway/config.hpp"
#include "euxis/gateway/server.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

// Top-level fail-fast: see apps/cli/src/main.cpp for the rationale.
// NOLINTBEGIN(bugprone-exception-escape)
int main(int argc, char* argv[]) {
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

    auto config = euxis::gateway::GatewayConfig::load_from_file(config_path);
    euxis::gateway::GatewayServer server(std::move(config));
    server.start();

    return 0;
}
// NOLINTEND(bugprone-exception-escape)
