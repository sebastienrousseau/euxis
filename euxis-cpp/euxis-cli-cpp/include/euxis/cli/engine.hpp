#pragma once

#include <string>
#include <vector>

namespace euxis::cli {

/// Dispatch CLI commands to the appropriate handler.
class Engine {
public:
    explicit Engine(const std::string& euxis_home = {});

    /// Run a command. Returns exit code.
    auto run(const std::vector<std::string>& args) -> int;

    /// Run a bridge sub-command.
    auto run_bridge(const std::vector<std::string>& args) -> int;

    /// Start the gateway server.
    auto run_gateway(const std::vector<std::string>& args) -> int;

    /// Display version info.
    static void print_version();

private:
    std::string euxis_home_;
};

} // namespace euxis::cli
