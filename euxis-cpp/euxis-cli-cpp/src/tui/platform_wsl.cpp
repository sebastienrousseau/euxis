#include "euxis/cli/tui/platform.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace euxis::cli::tui {

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
// WSLPlatform
// ---------------------------------------------------------------------------

WSLPlatform::WSLPlatform() {
    // Detect WSL version: WSL2 has a real Linux kernel, WSL1 uses Windows kernel.
    // /proc/version for WSL2 contains "microsoft-standard-WSL2"
    std::ifstream proc_version("/proc/version");
    if (proc_version.is_open()) {
        std::string line;
        std::getline(proc_version, line);
        is_wsl2_ = (line.find("WSL2") != std::string::npos ||
                     line.find("microsoft-standard") != std::string::npos);
    }
}

bool WSLPlatform::clipboard_copy(std::string_view text) const {
    return run_pipe_command("clip.exe 2>/dev/null", text);
}

std::string WSLPlatform::clipboard_paste() const {
    // PowerShell Get-Clipboard, but clean up the \r\n line endings
    std::string result = run_command("powershell.exe -NoProfile -Command Get-Clipboard 2>/dev/null");
    // Remove carriage returns
    std::string clean;
    clean.reserve(result.size());
    for (char c : result) {
        if (c != '\r') clean += c;
    }
    return clean;
}

bool WSLPlatform::open_url(std::string_view url) const {
    // Try wslview (from wslu) first, fall back to cmd.exe
    std::string cmd = "wslview '" + std::string(url) + "' 2>/dev/null";
    if (std::system(cmd.c_str()) == 0) return true;

    cmd = "cmd.exe /c start '" + std::string(url) + "' 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

bool WSLPlatform::supports_truecolor() const {
    // Windows Terminal supports truecolor; legacy ConHost does not.
    const char* wt = std::getenv("WT_SESSION");
    if (wt) return true;

    const char* ct = std::getenv("COLORTERM");
    if (ct) {
        std::string_view sv(ct);
        return sv == "truecolor" || sv == "24bit";
    }
    return false;
}

} // namespace euxis::cli::tui
