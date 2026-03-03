#include "euxis/cli/terminal.hpp"
#include "euxis/cli/i18n.hpp"
#include <cstdio>
#include <iostream>
#include <format>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

namespace euxis::cli::terminal {

bool colors_enabled() {
    return isatty(STDOUT_FILENO);
}

bool is_ci() {
    return std::getenv("CI") != nullptr || std::getenv("GITHUB_ACTIONS") != nullptr;
}

auto bold(std::string_view text) -> std::string {
    if (!colors_enabled()) return std::string(text);
    return std::format("\033[1m{}\033[0m", text);
}

auto dim(std::string_view text) -> std::string {
    if (!colors_enabled()) return std::string(text);
    return std::format("\033[2m{}\033[0m", text);
}

auto red(std::string_view text) -> std::string {
    if (!colors_enabled()) return std::string(text);
    return std::format("\033[31m{}\033[0m", text);
}

auto green(std::string_view text) -> std::string {
    if (!colors_enabled()) return std::string(text);
    return std::format("\033[32m{}\033[0m", text);
}

auto yellow(std::string_view text) -> std::string {
    if (!colors_enabled()) return std::string(text);
    return std::format("\033[33m{}\033[0m", text);
}

auto blue(std::string_view text) -> std::string {
    if (!colors_enabled()) return std::string(text);
    return std::format("\033[34m{}\033[0m", text);
}

auto cyan(std::string_view text) -> std::string {
    if (!colors_enabled()) return std::string(text);
    return std::format("\033[36m{}\033[0m", text);
}

auto magenta(std::string_view text) -> std::string {
    if (!colors_enabled()) return std::string(text);
    return std::format("\033[35m{}\033[0m", text);
}

auto icon_ok() -> std::string { return green("✔"); }
auto icon_fail() -> std::string { return red("✘"); }
auto icon_warn() -> std::string { return yellow("⚠"); }
auto icon_skip() -> std::string { return dim("↷"); }
auto icon_info() -> std::string { return blue("ℹ"); }

void spinner_frame(int frame, std::string_view message) {
    static const std::vector<std::string> frames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    std::print(stderr, "\r{} {}", cyan(frames[frame % frames.size()]), message);
}

void spinner_clear() {
    std::print(stderr, "\r\033[K");
}

void print_table(const std::vector<std::string>& headers, const std::vector<TableRow>& rows) {
    if (headers.empty()) return;
    std::vector<size_t> widths(headers.size());
    for (size_t i = 0; i < headers.size(); ++i) widths[i] = headers[i].size();
    for (const auto& row : rows) {
        for (size_t i = 0; i < headers.size() && i < row.cells.size(); ++i) {
            widths[i] = std::max(widths[i], row.cells[i].size());
        }
    }
    for (size_t i = 0; i < headers.size(); ++i) {
        std::print("{}", bold(headers[i]));
        if (i + 1 < headers.size()) std::print("{}", std::string(widths[i] - headers[i].size() + 2, ' '));
    }
    std::println("");
    for (const auto& row : rows) {
        for (size_t i = 0; i < headers.size(); ++i) {
            const auto& cell = (i < row.cells.size()) ? row.cells[i] : "";
            std::print("{}", cell);
            if (i + 1 < headers.size()) std::print("{}", std::string(widths[i] - cell.size() + 2, ' '));
        }
        std::println("");
    }
}

void progress_bar(int current, int total, std::string_view label) {
    if (total <= 0) return;
    constexpr int kBarWidth = 30;
    int filled = (current * kBarWidth) / total;
    std::print(stderr, "\r{} [", label);
    for (int i = 0; i < kBarWidth; ++i) std::print(stderr, "{}", (i < filled ? "█" : "░"));
    std::print(stderr, "] {}%", (current * 100) / total);
    if (current >= total) std::println(stderr, "");
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
    return std::format("\033[38;2;{};{};{}m{}\033[0m", r, g, b, text);
}

auto rgb_bg(uint8_t r, uint8_t g, uint8_t b, std::string_view text) -> std::string {
    return std::format("\033[48;2;{};{};{}m{}\033[0m", r, g, b, text);
}

static struct termios orig_termios;
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0; 
    raw.c_cc[VTIME] = 1; 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode() {
    std::print("\033[?25h"); // Show cursor
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

int read_key() {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) != 1) return 0;
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return 1001;
                case 'B': return 1002;
                case 'C': return 1003;
                case 'D': return 1004;
            }
        }
        return '\x1b';
    }
    return c;
}

void get_terminal_size(int& width, int& height) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0) {
        width = 80; height = 24;
    } else {
        width = w.ws_col; height = w.ws_row;
    }
}

TerminalScreen::TerminalScreen() {
    get_terminal_size(width_, height_);
    front_buffer_.assign(width_ * height_, Cell{.ch = 0xFFFFFFFF});
    back_buffer_.resize(width_ * height_);
}

TerminalScreen::~TerminalScreen() {
    std::print("\033[?25h"); // Restore cursor
}

void TerminalScreen::resize(int w, int h) {
    if (w == width_ && h == height_) return;
    width_ = w; height_ = h;
    front_buffer_.assign(w * h, Cell{.ch = 0xFFFFFFFF});
    back_buffer_.assign(w * h, Cell{});
}

void TerminalScreen::clear() {
    for (auto& c : back_buffer_) c = Cell{};
}

void TerminalScreen::set_cell(int x, int y, char32_t ch, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br, uint8_t bg, uint8_t bb, bool bold) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    back_buffer_[y * width_ + x] = {ch, fr, fg, fb, br, bg, bb, bold};
}

void TerminalScreen::write_text(int x, int y, std::string_view text, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br, uint8_t bg, uint8_t bb, bool bold) {
    int cx = x, cy = y;
    for (size_t i = 0; i < text.size(); ) {
        unsigned char c1 = static_cast<unsigned char>(text[i]);
        char32_t cp = ' '; size_t len = 1;
        if (c1 < 0x80) { cp = c1; len = 1; }
        else if ((c1 & 0xE0) == 0xC0 && i + 1 < text.size()) { cp = ((c1 & 0x1F) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3F); len = 2; }
        else if ((c1 & 0xF0) == 0xE0 && i + 2 < text.size()) { cp = ((c1 & 0x0F) << 12) | ((static_cast<unsigned char>(text[i+1]) & 0x3F) << 6) | (static_cast<unsigned char>(text[i+2]) & 0x3F); len = 3; }
        else if ((c1 & 0xF8) == 0xF0 && i + 3 < text.size()) { cp = ((c1 & 0x07) << 18) | ((static_cast<unsigned char>(text[i+1]) & 0x3F) << 12) | ((static_cast<unsigned char>(text[i+2]) & 0x3F) << 6) | (static_cast<unsigned char>(text[i+3]) & 0x3F); len = 4; }
        if (cp == '\n') { cx = x; cy++; }
        else {
            if (cx >= width_) { cx = 0; cy++; }
            if (cy >= height_) break;
            set_cell(cx++, cy, cp, fr, fg, fb, br, bg, bb, bold);
        }
        i += len;
    }
}

void TerminalScreen::write_gradient(int x, int y, std::string_view text, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
    std::vector<char32_t> codepoints;
    for (size_t i = 0; i < text.size(); ) {
        unsigned char c1 = static_cast<unsigned char>(text[i]);
        char32_t cp = ' '; size_t len = 1;
        if (c1 < 0x80) { cp = c1; len = 1; }
        else if ((c1 & 0xE0) == 0xC0 && i + 1 < text.size()) { cp = ((c1 & 0x1F) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3F); len = 2; }
        else if ((c1 & 0xF0) == 0xE0 && i + 2 < text.size()) { cp = ((c1 & 0x0F) << 12) | ((static_cast<unsigned char>(text[i+1]) & 0x3F) << 6) | (static_cast<unsigned char>(text[i+2]) & 0x3F); len = 3; }
        else if ((c1 & 0xF8) == 0xF0 && i + 3 < text.size()) { cp = ((c1 & 0x07) << 18) | ((static_cast<unsigned char>(text[i+1]) & 0x3F) << 12) | ((static_cast<unsigned char>(text[i+2]) & 0x3F) << 6) | (static_cast<unsigned char>(text[i+3]) & 0x3F); len = 4; }
        codepoints.push_back(cp); i += len;
    }
    int sz = static_cast<int>(codepoints.size());
    for (int i = 0; i < sz; ++i) {
        float t = (sz > 1) ? (float)i / (sz - 1) : 0;
        uint8_t r = (uint8_t)(r1 + t * (r2 - r1)), g = (uint8_t)(g1 + t * (g2 - g1)), b = (uint8_t)(b1 + t * (b2 - b1));
        set_cell(x + i, y, codepoints[i], r, g, b, 0, 0, 0, true);
    }
}

void TerminalScreen::draw_box(int x, int y, int w, int h, std::string_view title) {
    if (w < 2 || h < 2) return;
    set_cell(x, y, U'╭'); set_cell(x + w - 1, y, U'╮'); set_cell(x, y + h - 1, U'╰'); set_cell(x + w - 1, y + h - 1, U'╯');
    for (int i = 1; i < w - 1; ++i) { set_cell(x + i, y, U'─'); set_cell(x + i, y + h - 1, U'─'); }
    for (int i = 1; i < h - 1; ++i) { set_cell(x, y + i, U'│'); set_cell(x + w - 1, y + i, U'│'); }
    if (!title.empty()) write_text(x + 2, y, " " + std::string(title) + " ", 139, 233, 253, 0, 0, 0, true);
}

void TerminalScreen::render() {
    std::string out = "\033[?25l";
    uint8_t fr = 255, fg = 255, fb = 255, br = 0, bg = 0, bb = 0;
    bool in_c = false;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int idx = y * width_ + x;
            if (front_buffer_[idx] == back_buffer_[idx]) continue;
            out += std::format("\033[{};{}H", y + 1, x + 1);
            const auto& c = back_buffer_[idx];
            if (!in_c || c.fg_r != fr || c.fg_g != fg || c.fg_b != fb || c.bg_r != br || c.bg_g != bg || c.bg_b != bb) {
                out += std::format("\033[38;2;{};{};{}m\033[48;2;{};{};{}m", c.fg_r, c.fg_g, c.fg_b, c.bg_r, c.bg_g, c.bg_b);
                fr = c.fg_r; fg = c.fg_g; fb = c.fg_b; br = c.bg_r; bg = c.bg_g; bb = c.bg_b; in_c = true;
            }
            if (c.ch < 0x80) out += (char)c.ch;
            else if (c.ch < 0x800) { out += (char)(0xC0|(c.ch>>6)); out += (char)(0x80|(c.ch&0x3F)); }
            else if (c.ch < 0x10000) { out += (char)(0xE0|(c.ch>>12)); out += (char)(0x80|((c.ch>>6)&0x3F)); out += (char)(0x80|(c.ch&0x3F)); }
            else { out += (char)(0xF0|(c.ch>>18)); out += (char)(0x80|((c.ch>>12)&0x3F)); out += (char)(0x80|((c.ch>>6)&0x3F)); out += (char)(0x80|(c.ch&0x3F)); }
            front_buffer_[idx] = c;
        }
    }
    ::write(STDOUT_FILENO, out.data(), out.size());
}

} // namespace euxis::cli::terminal
