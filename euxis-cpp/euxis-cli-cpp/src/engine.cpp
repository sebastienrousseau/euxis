#include "euxis/cli/engine.hpp"
#include "euxis/gateway/server.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <csignal>

#include <spdlog/spdlog.h>

namespace euxis::cli {
namespace {

constexpr auto kVersion = "v0.0.3";

} // namespace

Engine::Engine(const std::string& euxis_home) {
    if (!euxis_home.empty()) {
        euxis_home_ = euxis_home;
    } else {
        const char* home = std::getenv("EUXIS_HOME");
        if (home) {
            euxis_home_ = home;
        } else {
            const char* user_home = std::getenv("HOME");
            euxis_home_ =
                (std::filesystem::path(user_home ? user_home : "/tmp") / ".euxis")
                    .string();
        }
    }
}

auto Engine::run(const std::vector<std::string>& args) -> int {
    if (args.empty()) {
        print_version();
        std::cout << "Usage: euxis <command> [options]\n"
                  << "Commands: version, bridge, gateway, help\n";
        return 0;
    }

    const auto& cmd = args[0];

    if (cmd == "version" || cmd == "--version" || cmd == "-V") {
        print_version();
        return 0;
    }

    if (cmd == "bridge") {
        return run_bridge({args.begin() + 1, args.end()});
    }

    if (cmd == "gateway") {
        return run_gateway({args.begin() + 1, args.end()});
    }

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_version();
        std::cout << "Usage: euxis <command> [options]\n"
                  << "Commands:\n"
                  << "  version    Show version\n"
                  << "  bridge     Bridge tool commands\n"
                  << "  gateway    Start gateway server\n"
                  << "  help       Show this help\n";
        return 0;
    }

    // Fallback: try to exec bash script
    auto bash_script = std::filesystem::path(euxis_home_) / "euxis-bin" / "euxis.sh";
    if (std::filesystem::exists(bash_script)) {
        std::string command = bash_script.string();
        for (const auto& arg : args) {
            command += " " + arg;
        }
        return std::system(command.c_str());
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    return 1;
}

auto Engine::run_bridge(const std::vector<std::string>& args) -> int {
    if (args.empty()) {
        std::cout << "Usage: euxis bridge <import-openclaw|daemon|keygen|...>\n";
        return 2;
    }
    // Bridge commands delegate to Python scripts in euxis-ops/bridge/
    auto script = std::filesystem::path(euxis_home_) / "euxis-ops" / "bridge" /
                  (args[0] + ".py");
    if (!std::filesystem::exists(script)) {
        std::cerr << "Bridge tool not found: " << script << "\n";
        return 1;
    }
    std::string command = "python3 " + script.string();
    for (size_t i = 1; i < args.size(); ++i) {
        command += " " + args[i];
    }
    return std::system(command.c_str());
}

auto Engine::run_gateway(const std::vector<std::string>& args) -> int {
    euxis::gateway::GatewayConfig config;

    // Parse optional --port / --ws-port / --host flags
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--port" && i + 1 < args.size()) {
            config.port = std::stoi(args[++i]);
        } else if (args[i] == "--ws-port" && i + 1 < args.size()) {
            config.ws_port = std::stoi(args[++i]);
        } else if (args[i] == "--host" && i + 1 < args.size()) {
            config.host = args[++i];
        } else if (args[i] == "--config" && i + 1 < args.size()) {
            config = euxis::gateway::GatewayConfig::load_from_file(args[++i]);
        }
    }

    euxis::gateway::GatewayServer server(config);

    // Handle SIGINT/SIGTERM for graceful shutdown
    static euxis::gateway::GatewayServer* server_ptr = &server;
    std::signal(SIGINT, [](int) { server_ptr->stop(); });
    std::signal(SIGTERM, [](int) { server_ptr->stop(); });

    server.start();
    return 0;
}

void Engine::print_version() {
    std::cout << "Euxis " << kVersion << " (C++23)\n";
}

} // namespace euxis::cli
