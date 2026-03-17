#include "euxis/cli/session.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace euxis::cli {

namespace {
    /// Validate that a path component has no traversal sequences.
    bool is_safe_path_component(const std::string& s) {
        if (s.empty()) return false;
        if (s == "." || s == "..") return false;
        if (s.find('/') != std::string::npos || s.find('\\') != std::string::npos) return false;
        if (s.find('\0') != std::string::npos) return false;
        return true;
    }
}

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

auto Session::euxis_home() const -> std::string {
    return euxis_home_;
}

auto Session::ensure_project_dirs(const std::string& agent_id) const -> std::string {
    auto project = project_name();
    if (!is_safe_path_component(project) || !is_safe_path_component(agent_id)) {
        throw std::runtime_error("invalid project name or agent id");
    }

    auto base = std::filesystem::path(euxis_home_) / "data" / "projects" / project;
    auto agent_dir = base / agent_id;
    auto output_dir = agent_dir / "output";

    std::filesystem::create_directories(output_dir);

    // Create audit.md and memory.md atomically (open with exclusive create, fall
    // through silently if they already exist — avoids TOCTOU race).
    auto audit_path = agent_dir / "audit.md";
    std::ofstream audit(audit_path, std::ios::app);
    if (audit.is_open() && std::filesystem::file_size(audit_path) == 0) {
        audit << "# Audit Log: " << agent_id << "\n";
    }
    auto memory_path = agent_dir / "memory.md";
    std::ofstream mem(memory_path, std::ios::app);
    if (mem.is_open() && std::filesystem::file_size(memory_path) == 0) {
        mem << "# Memory: " << agent_id << "\n";
    }

    return agent_dir.string();
}

auto Session::get_memory_context(const std::string& agent_id, int max_lines) const -> std::string {
    auto project = project_name();
    if (!is_safe_path_component(project) || !is_safe_path_component(agent_id)) return {};
    auto memory_path = std::filesystem::path(euxis_home_) / "data" / "projects" /
                       project / agent_id / "memory.md";

    if (!std::filesystem::exists(memory_path)) return {};

#if defined(__unix__) || defined(__APPLE__)
    // Zero-copy mmap implementation
    int fd = ::open(memory_path.c_str(), O_RDONLY);
    if (fd == -1) return {};

    struct stat sb;
    if (::fstat(fd, &sb) == -1 || sb.st_size == 0) {
        ::close(fd);
        return {};
    }

    void* mapped = ::mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd); // Can close immediately after mmap

    if (mapped == MAP_FAILED) return {};

    std::string_view data(static_cast<const char*>(mapped), sb.st_size);
    std::vector<std::string_view> all_lines;
    size_t pos = 0;
    while (pos < data.size()) {
        size_t end = data.find('\n', pos);
        if (end == std::string_view::npos) {
            all_lines.push_back(data.substr(pos));
            break;
        }
        all_lines.push_back(data.substr(pos, end - pos));
        pos = end + 1;
    }

    std::ostringstream out;
    int start = std::max(0, static_cast<int>(all_lines.size()) - max_lines);
    for (int i = start; i < static_cast<int>(all_lines.size()); ++i) {
        out << all_lines[static_cast<size_t>(i)] << "\n";
    }

    ::munmap(mapped, sb.st_size);
    return out.str();
#else
    // Fallback for non-UNIX
    std::ifstream f(memory_path);
    std::vector<std::string> all_lines;
    std::string line;
    while (std::getline(f, line)) {
        all_lines.push_back(std::move(line));
    }

    std::ostringstream out;
    int start = std::max(0, static_cast<int>(all_lines.size()) - max_lines);
    for (int i = start; i < static_cast<int>(all_lines.size()); ++i) {
        out << all_lines[static_cast<size_t>(i)] << "\n";
    }
    return out.str();
#endif
}

void Session::save_memory(const std::string& agent_id, const std::string& user_msg, const std::string& assistant_msg) const {
    auto project = project_name();
    if (!is_safe_path_component(project) || !is_safe_path_component(agent_id)) return;
    auto agent_dir = std::filesystem::path(euxis_home_) / "data" / "projects" / project / agent_id;
    std::filesystem::create_directories(agent_dir);
    
    auto memory_path = agent_dir / "memory.md";
    std::ofstream f(memory_path, std::ios::app);
    if (f.is_open()) {
        f << "\n### " << session_id() << "\n"
          << "**User:** " << user_msg << "\n"
          << "**Assistant:** " << assistant_msg << "\n";
    }
}

} // namespace euxis::cli
