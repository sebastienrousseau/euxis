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

#include <sys/ioctl.h>

void get_terminal_size(int& width, int& height) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0) {
        width = 80;
        height = 24;
    } else {
        width = w.ws_col;
        height = w.ws_row;
    }
}

TerminalScreen::TerminalScreen() {
    get_terminal_size(width_, height_);
    front_buffer_.assign(width_ * height_, Cell{.ch = 0xFFFFFFFF}); // Dirty state forces full draw
    back_buffer_.resize(width_ * height_);
}

TerminalScreen::~TerminalScreen() {}

void TerminalScreen::resize(int w, int h) {
    if (w == width_ && h == height_) return;
    width_ = w;
    height_ = h;
    front_buffer_.assign(w * h, Cell{.ch = 0xFFFFFFFF}); // Dirty state
    back_buffer_.assign(w * h, Cell{});
}

void TerminalScreen::clear() {
    for (auto& c : back_buffer_) {
        c = Cell{};
    }
}

void TerminalScreen::set_cell(int x, int y, char32_t ch, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br, uint8_t bg, uint8_t bb, bool bold) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    int idx = y * width_ + x;
    back_buffer_[idx] = {ch, fr, fg, fb, br, bg, bb, bold};
}

void TerminalScreen::write_text(int x, int y, std::string_view text, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br, uint8_t bg, uint8_t bb, bool bold) {
    int cx = x;
    int cy = y;
    
    for (size_t i = 0; i < text.size(); ) {
        unsigned char c1 = static_cast<unsigned char>(text[i]);
        char32_t codepoint = ' ';
        size_t len = 1;

        if (c1 < 0x80) { codepoint = c1; len = 1; }
        else if ((c1 & 0xE0) == 0xC0) {
            if (i + 1 < text.size()) {
                codepoint = ((c1 & 0x1F) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3F);
                len = 2;
            }
        } else if ((c1 & 0xF0) == 0xE0) {
            if (i + 2 < text.size()) {
                codepoint = ((c1 & 0x0F) << 12) | ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6) | (static_cast<unsigned char>(text[i + 2]) & 0x3F);
                len = 3;
            }
        } else if ((c1 & 0xF8) == 0xF0) {
            if (i + 3 < text.size()) {
                codepoint = ((c1 & 0x07) << 18) | ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12) | ((static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6) | (static_cast<unsigned char>(text[i + 3]) & 0x3F);
                len = 4;
            }
        }

        if (codepoint == '\n') {
            cx = x; cy++;
        } else {
            if (cx >= width_) { cx = 0; cy++; }
            if (cy >= height_) break;
            set_cell(cx++, cy, codepoint, fr, fg, fb, br, bg, bb, bold);
        }
        i += len;
    }
}

void TerminalScreen::write_gradient(int x, int y, std::string_view text, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
    int len = static_cast<int>(text.size());
    if (len == 0) return;
    for (int i = 0; i < len; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(len);
        uint8_t r = static_cast<uint8_t>(r1 + t * (r2 - r1));
        uint8_t g = static_cast<uint8_t>(g1 + t * (g2 - g1));
        uint8_t b = static_cast<uint8_t>(b1 + t * (b2 - b1));
        set_cell(x + i, y, text[i], r, g, b, 0, 0, 0, true);
    }
}

void TerminalScreen::draw_box(int x, int y, int w, int h, std::string_view title) {
    if (w < 2 || h < 2) return;
    // Use high-quality Unicode box characters
    char32_t tl = U'╭', tr = U'╮', bl = U'╰', br = U'╯';
    char32_t hline = U'─', vline = U'│';

    // Fallback if colors/unicode disabled (basic check)
    if (!colors_enabled()) {
        tl = tr = bl = br = '+';
        hline = '-'; vline = '|';
    }

    set_cell(x, y, tl);
    set_cell(x + w - 1, y, tr);
    set_cell(x, y + h - 1, bl);
    set_cell(x + w - 1, y + h - 1, br);

    for (int i = 1; i < w - 1; ++i) {
        set_cell(x + i, y, hline);
        set_cell(x + i, y + h - 1, hline);
    }
    for (int i = 1; i < h - 1; ++i) {
        set_cell(x, y + i, vline);
        set_cell(x + w - 1, y + i, vline);
    }

    if (!title.empty()) {
        write_text(x + 2, y, " " + std::string(title) + " ", 139, 233, 253, 0, 0, 0, true);
    }
}

void TerminalScreen::render() {
    std::string out;
    out.reserve(width_ * height_ * 10);

    bool in_color = false;
    uint8_t cur_fr = 255, cur_fg = 255, cur_fb = 255;
    uint8_t cur_br = 0, cur_bg = 0, cur_bb = 0;

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int idx = y * width_ + x;
            if (front_buffer_[idx] == back_buffer_[idx]) continue; // No change

            // Move cursor
            out += std::format("\033[{};{}H", y + 1, x + 1);

            const auto& c = back_buffer_[idx];
            
            // Output colors only if changed
            if (c.fg_r != cur_fr || c.fg_g != cur_fg || c.fg_b != cur_fb ||
                c.bg_r != cur_br || c.bg_g != cur_bg || c.bg_b != cur_bb || !in_color) {
                out += std::format("\033[38;2;{};{};{}m\033[48;2;{};{};{}m", 
                                   c.fg_r, c.fg_g, c.fg_b, c.bg_r, c.bg_g, c.bg_b);
                cur_fr = c.fg_r; cur_fg = c.fg_g; cur_fb = c.fg_b;
                cur_br = c.bg_r; cur_bg = c.bg_g; cur_bb = c.bg_b;
                in_color = true;
            }

            // Proper UTF-8 encoding for the terminal
            if (c.ch < 0x80) {
                out += static_cast<char>(c.ch);
            } else if (c.ch < 0x800) {
                out += static_cast<char>(0xC0 | (c.ch >> 6));
                out += static_cast<char>(0x80 | (c.ch & 0x3F));
            } else if (c.ch < 0x10000) {
                out += static_cast<char>(0xE0 | (c.ch >> 12));
                out += static_cast<char>(0x80 | ((c.ch >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (c.ch & 0x3F));
            } else {
                out += static_cast<char>(0xF0 | (c.ch >> 18));
                out += static_cast<char>(0x80 | ((c.ch >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((c.ch >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (c.ch & 0x3F));
            }

            front_buffer_[idx] = c;
        }
    }
    
    if (!out.empty()) {
        ::write(STDOUT_FILENO, out.data(), out.size());
    }
}

} // namespace euxis::cli::terminal
