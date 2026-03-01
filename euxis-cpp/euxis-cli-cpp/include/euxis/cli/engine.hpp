#pragma once

#include "euxis/cli/command.hpp"

#include <string>
#include <vector>

namespace euxis::cli {

/// Table-driven CLI engine. Dispatches commands by name, generates grouped help.
class Engine {
public:
    explicit Engine(const std::string& euxis_home = {});

    /// Run a command. Returns exit code.
    auto run(const std::vector<std::string>& args) -> int;

    /// Display version info.
    static void print_version();

    /// Display grouped help for all registered commands.
    void print_help() const;

private:
    Context ctx_;
    std::vector<CommandEntry> commands_;

    void register_commands();
};

} // namespace euxis::cli
