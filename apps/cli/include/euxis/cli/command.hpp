#pragma once

#include <string>
#include <vector>

namespace euxis::cli {

/// Shared context passed to every command handler.
struct Context {
    std::string euxis_home;
    std::string data_dir;   // data/
    bool no_color{false};
    bool json_output{false};
    bool verbose{false};
};

/// Signature for a command handler: returns exit code.
using CommandHandler = int (*)(Context& ctx, const std::vector<std::string>& args);

/// Table entry describing a single CLI sub-command.
struct CommandEntry {
    std::string name;
    std::string group;       // System, Fleet, Knowledge, Infrastructure, Development, Specialized
    std::string description;
    CommandHandler handler;
};

} // namespace euxis::cli
