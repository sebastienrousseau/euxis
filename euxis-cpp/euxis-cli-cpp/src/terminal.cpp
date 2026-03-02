#include "euxis/cli/terminal.hpp"
#include "euxis/cli/term_caps.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <print>

#include <unistd.h>

namespace euxis::cli::terminal {
namespace {

constexpr std::string_view kSpinnerFrames[] = {
    "\xe2\xa0\x8b", "\xe2\xa0\x99", "\xe2\xa0\xb9", "\xe2\xa0\xb8",
    "\xe2\xa0\xbc", "\xe2\xa0\xb4", "\xe2\xa0\xa6", "\xe2\xa0\xa7",
    "\xe2\xa0\x87", "\xe2\xa0\x8f"  // braille spinner
};
constexpr int kSpinnerFrameCount = 10;

} // namespace

bool colors_enabled() {
    static const bool enabled = [] {
        if (std::getenv("NO_COLOR")) { return false; }
        if (std::getenv("FORCE_COLOR")) { return true; }
        const char* ui = std::getenv("EUXIS_UI");
        if (ui && std::string_view(ui) == "0") { return false; }
        return isatty(STDOUT_FILENO) != 0;
    }();
    return enabled;
}

bool is_ci() {
    static const bool ci = std::getenv("CI") || std::getenv("GITHUB_ACTIONS") ||
                           std::getenv("GITLAB_CI") || std::getenv("JENKINS_URL");
    return ci;
}

namespace {

auto wrap(std::string_view code, std::string_view text) -> std::string {
    if (!colors_enabled()) { return std::string(text); }
    return std::format("\033[{}m{}\033[0m", code, text);
}

} // namespace

auto bold(std::string_view text) -> std::string    { return wrap("1", text); }
auto dim(std::string_view text) -> std::string     { return wrap("2", text); }
auto red(std::string_view text) -> std::string     { return wrap("31", text); }
auto green(std::string_view text) -> std::string   { return wrap("32", text); }
auto yellow(std::string_view text) -> std::string  { return wrap("33", text); }
auto blue(std::string_view text) -> std::string    { return wrap("34", text); }
auto magenta(std::string_view text) -> std::string { return wrap("35", text); }
auto cyan(std::string_view text) -> std::string    { return wrap("36", text); }

auto icon_ok() -> std::string   { return colors_enabled() ? green("\xe2\x9c\x93") : "[OK]"; }
auto icon_fail() -> std::string { return colors_enabled() ? red("\xe2\x9c\x97") : "[FAIL]"; }
auto icon_warn() -> std::string { return colors_enabled() ? yellow("\xe2\x9a\xa0") : "[WARN]"; }
auto icon_skip() -> std::string { return colors_enabled() ? dim("-") : "[-]"; }
auto icon_info() -> std::string { return colors_enabled() ? blue("\xe2\x84\xb9") : "[i]"; }

void spinner_frame(int frame, std::string_view message) {
    int idx = frame % kSpinnerFrameCount;
    std::print(stderr, "\r{} {}  ", kSpinnerFrames[static_cast<size_t>(idx)], message);
}

void spinner_clear() {
    std::print(stderr, "\r\033[K");
}

void print_table(const std::vector<std::string>& headers,
                 const std::vector<TableRow>& rows) {
    if (headers.empty()) { return; }

    // Calculate column widths
    std::vector<size_t> widths(headers.size());
    for (size_t i = 0; i < headers.size(); ++i) {
        widths[i] = headers[i].size();
    }

    for (const auto& row : rows) {
        for (size_t i = 0; i < std::min(row.cells.size(), headers.size()); ++i) {
            widths[i] = std::max(widths[i], row.cells[i].size());
        }
    }

    // Print header
    for (size_t i = 0; i < headers.size(); ++i) {
        std::print("{}", bold(headers[i]));
        if (i + 1 < headers.size()) {
            std::print("{}", std::string(widths[i] - headers[i].size() + 2, ' '));
        }
    }
    std::println("");

    // Separator
    for (size_t i = 0; i < headers.size(); ++i) {
        std::print("{}", std::string(widths[i], '-'));
        if (i + 1 < headers.size()) {
            std::print("  ");
        }
    }
    std::println("");

    // Rows
    for (const auto& row : rows) {
        for (size_t i = 0; i < headers.size(); ++i) {
            const auto& cell = (i < row.cells.size()) ? row.cells[i] : "";
            std::print("{}", cell);
            if (i + 1 < headers.size()) {
                std::print("{}", std::string(widths[i] - cell.size() + 2, ' '));
            }
        }
        std::println("");
    }
}

void progress_bar(int current, int total, std::string_view label) {
    if (total <= 0) { return; }
    constexpr int kBarWidth = 30;
    int filled = (current * kBarWidth) / total;
    int pct = (current * 100) / total;

    std::print(stderr, "\r{} [", label);
    for (int i = 0; i < kBarWidth; ++i) {
        std::print(stderr, "{}", (i < filled ? "\xe2\x96\x88" : "\xe2\x96\x91"));
    }
    std::print(stderr, "] {}%", pct);

    if (current >= total) {
        std::println(stderr, "");
    }
}

void print_banner() {
    if (colors_enabled()) {
        std::println("{}", cyan(R"(
  ███████╗██╗   ██╗██╗  ██╗██╗███████╗
  ██╔════╝██║   ██║╚██╗██╔╝██║██╔════╝
  █████╗  ██║   ██║ ╚███╔╝ ██║███████╗
  ██╔══╝  ██║   ██║ ██╔██╗ ██║╚════██║
  ███████╗╚██████╔╝██╔╝ ██╗██║███████║
  ╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝╚══════╝
)"));
    } else {
        std::println("  EUXIS\n");
    }
}

auto rgb_fg(uint8_t r, uint8_t g, uint8_t b, std::string_view text) -> std::string {
    if (!colors_enabled() || !caps().truecolor) { return std::string(text); }
    return std::format("\033[38;2;{};{};{}m{}\033[0m", r, g, b, text);
}

auto rgb_bg(uint8_t r, uint8_t g, uint8_t b, std::string_view text) -> std::string {
    if (!colors_enabled() || !caps().truecolor) { return std::string(text); }
    return std::format("\033[48;2;{};{};{}m{}\033[0m", r, g, b, text);
}

// --- Raw Mode & Input ---
#include <termios.h>

static struct termios orig_termios;

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    // Disable echo, canonical mode (line-by-line), and signals (Ctrl-C)
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    // Allow non-blocking reads or block until 1 char
    raw.c_cc[VMIN] = 0; 
    raw.c_cc[VTIME] = 1; // 100ms timeout for poll-like behavior
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

int read_key() {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) != 1) return 0; // Timeout or nothing
    
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    if (seq[1] == '3') return 1000; // Delete
                }
            } else {
                switch (seq[1]) {
                    case 'A': return 1001; // Up
                    case 'B': return 1002; // Down
                    case 'C': return 1003; // Right
                    case 'D': return 1004; // Left
                }
            }
        }
        return '\x1b';
    }
    return c;
}

} // namespace euxis::cli::terminal
