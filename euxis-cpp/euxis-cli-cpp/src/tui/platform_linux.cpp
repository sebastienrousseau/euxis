#include "euxis/cli/tui/platform.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

namespace euxis::cli::tui {

// ---------------------------------------------------------------------------
// Platform detection (shared across all platform files)
// ---------------------------------------------------------------------------

PlatformType Platform::detect_type() {
#ifdef __APPLE__
    return PlatformType::MacOS;
#else
    // Check for WSL
    if (std::getenv("WSL_DISTRO_NAME")) return PlatformType::WSL;

    std::ifstream proc_version("/proc/version");
    if (proc_version.is_open()) {
        std::string line;
        std::getline(proc_version, line);
        // WSL1/WSL2 both contain "microsoft" (case-insensitive)
        for (auto& ch : line) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (line.find("microsoft") != std::string::npos) return PlatformType::WSL;
    }

    return PlatformType::Linux;
#endif
}

std::unique_ptr<Platform> Platform::detect() {
    switch (detect_type()) {
        case PlatformType::MacOS: return std::make_unique<MacOSPlatform>();
        case PlatformType::WSL:   return std::make_unique<WSLPlatform>();
        default:                  return std::make_unique<LinuxPlatform>();
    }
}

// ---------------------------------------------------------------------------
// Helper: run a command and capture stdout
// ---------------------------------------------------------------------------
namespace {

std::string run_command(const std::string& cmd) {
    std::array<char, 4096> buffer{};
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        result += buffer.data();
    }
    pclose(pipe);
    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

bool run_pipe_command(const std::string& cmd, std::string_view input) {
    FILE* pipe = popen(cmd.c_str(), "w");
    if (!pipe) return false;
    fwrite(input.data(), 1, input.size(), pipe);
    return pclose(pipe) == 0;
}

} // namespace

// ---------------------------------------------------------------------------
// LinuxPlatform
// ---------------------------------------------------------------------------

bool LinuxPlatform::has_wayland() const {
    return std::getenv("WAYLAND_DISPLAY") != nullptr;
}

bool LinuxPlatform::clipboard_copy(std::string_view text) const {
    if (has_wayland()) {
        return run_pipe_command("wl-copy 2>/dev/null", text);
    }
    return run_pipe_command("xclip -selection clipboard 2>/dev/null", text);
}

std::string LinuxPlatform::clipboard_paste() const {
    if (has_wayland()) {
        return run_command("wl-paste 2>/dev/null");
    }
    return run_command("xclip -selection clipboard -o 2>/dev/null");
}

bool LinuxPlatform::open_url(std::string_view url) const {
    std::string cmd = "xdg-open '" + std::string(url) + "' >/dev/null 2>&1 &";
    return std::system(cmd.c_str()) == 0;
}

bool LinuxPlatform::supports_truecolor() const {
    const char* ct = std::getenv("COLORTERM");
    if (ct) {
        std::string_view sv(ct);
        return sv == "truecolor" || sv == "24bit";
    }
    return false;
}

} // namespace euxis::cli::tui
