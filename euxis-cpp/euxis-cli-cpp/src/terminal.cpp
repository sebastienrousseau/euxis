#include "euxis/cli/terminal.hpp"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

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
        if (std::getenv("NO_COLOR")) return false;
        if (std::getenv("FORCE_COLOR")) return true;
        const char* ui = std::getenv("EUXIS_UI");
        if (ui && std::string_view(ui) == "0") return false;
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
    if (!colors_enabled()) return std::string(text);
    return std::string("\033[") + std::string(code) + "m" +
           std::string(text) + "\033[0m";
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
    std::cerr << "\r" << kSpinnerFrames[idx] << " " << message << "  " << std::flush;
}

void spinner_clear() {
    std::cerr << "\r\033[K" << std::flush;
}

void print_table(const std::vector<std::string>& headers,
                 const std::vector<TableRow>& rows) {
    if (headers.empty()) return;

    // Calculate column widths
    std::vector<size_t> widths(headers.size());
    for (size_t i = 0; i < headers.size(); ++i)
        widths[i] = headers[i].size();

    for (const auto& row : rows) {
        for (size_t i = 0; i < std::min(row.cells.size(), headers.size()); ++i)
            widths[i] = std::max(widths[i], row.cells[i].size());
    }

    // Print header
    for (size_t i = 0; i < headers.size(); ++i) {
        std::cout << bold(headers[i]);
        if (i + 1 < headers.size())
            std::cout << std::string(widths[i] - headers[i].size() + 2, ' ');
    }
    std::cout << "\n";

    // Separator
    for (size_t i = 0; i < headers.size(); ++i) {
        std::cout << std::string(widths[i], '-');
        if (i + 1 < headers.size()) std::cout << "  ";
    }
    std::cout << "\n";

    // Rows
    for (const auto& row : rows) {
        for (size_t i = 0; i < headers.size(); ++i) {
            const auto& cell = (i < row.cells.size()) ? row.cells[i] : "";
            std::cout << cell;
            if (i + 1 < headers.size())
                std::cout << std::string(widths[i] - cell.size() + 2, ' ');
        }
        std::cout << "\n";
    }
}

void progress_bar(int current, int total, std::string_view label) {
    if (total <= 0) return;
    constexpr int kBarWidth = 30;
    int filled = (current * kBarWidth) / total;
    int pct = (current * 100) / total;

    std::cerr << "\r" << label << " [";
    for (int i = 0; i < kBarWidth; ++i)
        std::cerr << (i < filled ? "\xe2\x96\x88" : "\xe2\x96\x91");
    std::cerr << "] " << pct << "%" << std::flush;

    if (current >= total) std::cerr << "\n";
}

void print_banner() {
    if (colors_enabled()) {
        std::cout << cyan(R"(
  ███████╗██╗   ██╗██╗  ██╗██╗███████╗
  ██╔════╝██║   ██║╚██╗██╔╝██║██╔════╝
  █████╗  ██║   ██║ ╚███╔╝ ██║███████╗
  ██╔══╝  ██║   ██║ ██╔██╗ ██║╚════██║
  ███████╗╚██████╔╝██╔╝ ██╗██║███████║
  ╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝╚══════╝
)") << "\n";
    } else {
        std::cout << "  EUXIS\n\n";
    }
}

} // namespace euxis::cli::terminal
