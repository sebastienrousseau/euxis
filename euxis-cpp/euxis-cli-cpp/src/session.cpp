#include "euxis/cli/session.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace euxis::cli {

Session::Session(const std::string& euxis_home) : euxis_home_(euxis_home) {}

auto Session::project_name() const -> std::string {
    const char* proj = std::getenv("EUXIS_PROJECT");
    if (proj && proj[0]) return proj;

    auto cwd = std::filesystem::current_path();
    return cwd.filename().string();
}

auto Session::session_id() const -> std::string {
    const char* sid = std::getenv("EUXIS_SESSION_ID");
    if (sid && sid[0]) return sid;

    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
                     now.time_since_epoch())
                     .count();
    return std::to_string(epoch);
}

auto Session::ensure_project_dirs(const std::string& agent_id) const -> std::string {
    auto project = project_name();
    auto base = std::filesystem::path(euxis_home_) / "euxis-data" / "projects" / project;
    auto agent_dir = base / agent_id;
    auto output_dir = agent_dir / "output";

    std::filesystem::create_directories(output_dir);

    // Create audit.md and memory.md if they don't exist
    auto audit_path = agent_dir / "audit.md";
    if (!std::filesystem::exists(audit_path)) {
        std::ofstream(audit_path) << "# Audit Log: " << agent_id << "\n";
    }
    auto memory_path = agent_dir / "memory.md";
    if (!std::filesystem::exists(memory_path)) {
        std::ofstream(memory_path) << "# Memory: " << agent_id << "\n";
    }

    return agent_dir.string();
}

auto Session::get_memory_context(const std::string& agent_id, int lines) const -> std::string {
    auto project = project_name();
    auto memory_path = std::filesystem::path(euxis_home_) / "euxis-data" / "projects" /
                       project / agent_id / "memory.md";

    if (!std::filesystem::exists(memory_path)) return {};

    std::ifstream f(memory_path);
    std::vector<std::string> all_lines;
    std::string line;
    while (std::getline(f, line)) {
        all_lines.push_back(std::move(line));
    }

    std::ostringstream out;
    int start = std::max(0, static_cast<int>(all_lines.size()) - lines);
    for (int i = start; i < static_cast<int>(all_lines.size()); ++i) {
        out << all_lines[static_cast<size_t>(i)] << "\n";
    }
    return out.str();
}

} // namespace euxis::cli
