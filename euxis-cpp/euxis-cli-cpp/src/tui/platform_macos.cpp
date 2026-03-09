#include "euxis/cli/tui/platform.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
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
// MacOSPlatform
// ---------------------------------------------------------------------------

bool MacOSPlatform::clipboard_copy(std::string_view text) const {
    return run_pipe_command("pbcopy", text);
}

std::string MacOSPlatform::clipboard_paste() const {
    return run_command("pbpaste");
}

bool MacOSPlatform::open_url(std::string_view url) const {
    std::string cmd = "open '" + std::string(url) + "' 2>/dev/null &";
    return std::system(cmd.c_str()) == 0;
}

bool MacOSPlatform::supports_truecolor() const {
    // Terminal.app only does 256 color. iTerm2/Kitty/Alacritty do truecolor.
    const char* term_program = std::getenv("TERM_PROGRAM");
    if (term_program) {
        std::string_view tp(term_program);
        if (tp == "Apple_Terminal") return false;
    }
    const char* ct = std::getenv("COLORTERM");
    if (ct) {
        std::string_view sv(ct);
        return sv == "truecolor" || sv == "24bit";
    }
    return false;
}

} // namespace euxis::cli::tui
