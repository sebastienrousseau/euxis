#include "euxis/cli/terminal.hpp"
#include <cstdio>
#include <cstring>
#include <format>
#include <iostream>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace euxis::cli::terminal {

static struct termios orig_termios;
static bool raw_mode_active = false;

bool colors_enabled() {
    return isatty(STDOUT_FILENO);
}

bool is_ci() {
    return std::getenv("GITHUB_ACTIONS") || std::getenv("CI");
}

auto bold(std::string_view text) -> std::string { return std::format("\033[1m{}\033[0m", text); }
auto dim(std::string_view text) -> std::string { return std::format("\033[2m{}\033[0m", text); }
auto red(std::string_view text) -> std::string { return std::format("\033[31m{}\033[0m", text); }
auto green(std::string_view text) -> std::string { return std::format("\033[32m{}\033[0m", text); }
auto yellow(std::string_view text) -> std::string { return std::format("\033[33m{}\033[0m", text); }
auto blue(std::string_view text) -> std::string { return std::format("\033[34m{}\033[0m", text); }
auto cyan(std::string_view text) -> std::string { return std::format("\033[36m{}\033[0m", text); }
auto magenta(std::string_view text) -> std::string { return std::format("\033[35m{}\033[0m", text); }

auto icon_ok() -> std::string { return green("✔"); }
auto icon_fail() -> std::string { return red("✘"); }
auto icon_warn() -> std::string { return yellow("⚠"); }
auto icon_skip() -> std::string { return dim("↷"); }
auto icon_info() -> std::string { return blue("ℹ"); }

auto format_duration(double ms) -> std::string {
    if (ms < 1.0) return std::format("{:.2f}µs", ms * 1000.0);
    if (ms < 1000.0) return std::format("{:.1f}ms", ms);
    
    double seconds = ms / 1000.0;
    if (seconds < 60.0) return std::format("{:.2f}s", seconds);
    
    double minutes = seconds / 60.0;
    if (minutes < 60.0) return std::format("{:.1f}min", minutes);
    
    double hours = minutes / 60.0;
    return std::format("{:.1f}h", hours);
}

void spinner_frame(int frame, std::string_view message) {
    static const char* frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    std::fprintf(stderr, "\r%s %.*s", frames[frame % 10], (int)message.size(), message.data());
    std::fflush(stderr);
}

void spinner_clear() {
    std::fprintf(stderr, "\r\033[K");
    std::fflush(stderr);
}

void progress_bar(int current, int total, std::string_view label) {
    int width = 20;
    float progress = (float)current / total;
    int filled = (int)(width * progress);
    std::string bar = "[";
    for (int i = 0; i < width; ++i) bar += (i < filled) ? "■" : " ";
    bar += "]";
    std::fprintf(stderr, "\r%s %s %d/%d", label.data(), bar.c_str(), current, total);
    std::fflush(stderr);
}

void print_banner() {
    std::cout << bold(cyan(" EUXIS Agentic Development Environment\n"));
}

void print_table(const std::vector<std::string>& headers, const std::vector<TableRow>& rows) {
    print_table(std::cout, headers, rows);
}

void print_table(std::ostream& os, const std::vector<std::string>& headers, const std::vector<TableRow>& rows) {
    for (const auto& h : headers) os << bold(h) << "\t";
    os << "\n";
    for (const auto& r : rows) {
        for (const auto& c : r.cells) os << c << "\t";
        os << "\n";
    }
}

auto rgb_fg(uint8_t r, uint8_t g, uint8_t b, std::string_view text) -> std::string {
    return std::format("\033[38;2;{};{};{}m{}\033[0m", r, g, b, text);
}

auto rgb_bg(uint8_t r, uint8_t g, uint8_t b, std::string_view text) -> std::string {
    return std::format("\033[48;2;{};{};{}m{}\033[0m", r, g, b, text);
}

void enable_raw_mode() {
    if (raw_mode_active) return;
    if (::tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;
    struct termios raw = orig_termios;
    // Keep OPOST so \n still performs \r\n automatically
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return;
    raw_mode_active = true;
}

void disable_raw_mode() {
    if (!raw_mode_active) return;
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    raw_mode_active = false;
}

int read_key() {
    unsigned char c;
    if (::read(STDIN_FILENO, &c, 1) != 1) return 0;
    return static_cast<int>(c);
}

void get_terminal_size(int& w, int& h) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) { w = 80; h = 24; }
    else { w = ws.ws_col; h = ws.ws_row; }
}

TerminalScreen::TerminalScreen() {
    get_terminal_size(width_, height_);
    front_buffer_.assign(width_ * height_, Cell{.ch = 0xFFFFFFFF});
    back_buffer_.resize(width_ * height_);
}

TerminalScreen::~TerminalScreen() { disable_raw_mode(); }

void TerminalScreen::resize(int w, int h) {
    if (w == width_ && h == height_) return;
    width_ = w; height_ = h;
    front_buffer_.assign(w * h, Cell{.ch = 0xFFFFFFFF});
    back_buffer_.assign(w * h, Cell{});
}

void TerminalScreen::clear() { for (auto& c : back_buffer_) c = Cell{}; }

void TerminalScreen::set_cell(int x, int y, char32_t ch, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br, uint8_t bg, uint8_t bb, bool bld) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    back_buffer_[y * width_ + x] = {ch, fr, fg, fb, br, bg, bb, bld};
}

void TerminalScreen::write_text(int x, int y, std::string_view text, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br, uint8_t bg, uint8_t bb, bool bld) {
    int cx = x, cy = y;
    uint8_t curr_fr = fr, curr_fg = fg, curr_fb = fb;
    for (size_t i = 0; i < text.size();) {
        if ((unsigned char)text[i] == 0x1b && i + 2 < text.size() && text[i+1] == '[') {
            size_t m_pos = text.find('m', i + 2);
            if (m_pos != std::string::npos) {
                std::string_view sgr = text.substr(i + 2, m_pos - (i + 2));
                if (sgr == "0") { curr_fr = fr; curr_fg = fg; curr_fb = fb; }
                else if (sgr.starts_with("38;2;")) {
                    int r, g, b; if (std::sscanf(std::string(sgr).c_str(), "38;2;%d;%d;%d", &r, &g, &b) == 3) {
                        curr_fr = (uint8_t)r; curr_fg = (uint8_t)g; curr_fb = (uint8_t)b;
                    }
                }
                i = m_pos + 1; continue;
            }
        }
        unsigned char c1 = (unsigned char)text[i];
        char32_t cp = ' '; size_t len = 1;
        if (c1 < 0x80) { cp = c1; len = 1; }
        else if ((c1 & 0xE0) == 0xC0 && i + 1 < text.size()) { cp = ((c1 & 0x1F) << 6) | ((unsigned char)text[i+1] & 0x3F); len = 2; }
        else if ((c1 & 0xF0) == 0xE0 && i + 2 < text.size()) { cp = ((c1 & 0x0F) << 12) | (((unsigned char)text[i+1] & 0x3F) << 6) | ((unsigned char)text[i+2] & 0x3F); len = 3; }
        else if ((c1 & 0xF8) == 0xF0 && i + 3 < text.size()) { cp = ((c1 & 0x07) << 18) | (((unsigned char)text[i+1] & 0x3F) << 12) | (((unsigned char)text[i+2] & 0x3F) << 6) | (((unsigned char)text[i+3] & 0x3F)); len = 4; }
        if (cp == '\n') { cx = x; cy++; }
        else {
            if (cx >= width_) { cx = x; cy++; }
            if (cy < height_ && cy >= 0) set_cell(cx++, cy, cp, curr_fr, curr_fg, curr_fb, br, bg, bb, bld);
            else if (cy >= height_) break;
        }
        i += len;
    }
}

void TerminalScreen::render() {
    std::string out; uint8_t fr = 255, fg = 255, fb = 255, br = 0, bg = 0, bb = 0; bool in_c = false;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int idx = y * width_ + x; if (front_buffer_[idx] == back_buffer_[idx]) continue;
            out += std::format("\033[{};{}H", y + 1, x + 1);
            const auto& c = back_buffer_[idx];
            if (!in_c || c.fg_r != fr || c.fg_g != fg || c.fg_b != fb || c.bg_r != br || c.bg_g != bg || c.bg_b != bb) {
                out += std::format("\033[38;2;{};{};{}m\033[48;2;{};{};{}m", c.fg_r, c.fg_g, c.fg_b, c.bg_r, c.bg_g, c.bg_b);
                fr = c.fg_r; fg = c.fg_g; fb = c.fg_b; br = c.bg_r; bg = c.bg_g; bb = c.bg_b; in_c = true;
            }
            if (c.ch < 0x80) out += (char)c.ch;
            else if (c.ch < 0x800) { out += (char)(0xC0 | (c.ch >> 6)); out += (char)(0x80 | (c.ch & 0x3F)); }
            else if (c.ch < 0x10000) { out += (char)(0xE0 | (c.ch >> 12)); out += (char)(0x80 | ((c.ch >> 6) & 0x3F)); out += (char)(0x80 | (c.ch & 0x3F)); }
            else { out += (char)(0xF0 | (c.ch >> 18)); out += (char)(0x80 | ((c.ch >> 12) & 0x3F)); out += (char)(0x80 | ((c.ch >> 6) & 0x3F)); out += (char)(0x80 | (c.ch & 0x3F)); }
            front_buffer_[idx] = c;
        }
    }
    if (!out.empty()) [[maybe_unused]] auto _ = ::write(STDOUT_FILENO, out.data(), out.size());
}

void TerminalScreen::write_gradient(int x, int y, std::string_view text, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
    std::vector<char32_t> cps;
    for (size_t i = 0; i < text.size();) {
        unsigned char c1 = (unsigned char)text[i];
        char32_t cp = ' '; size_t len = 1;
        if (c1 < 0x80) { cp = c1; len = 1; }
        else if ((c1 & 0xE0) == 0xC0 && i + 1 < text.size()) { cp = ((c1 & 0x1F) << 6) | ((unsigned char)text[i+1] & 0x3F); len = 2; }
        else if ((c1 & 0xF0) == 0xE0 && i + 2 < text.size()) { cp = ((c1 & 0x0F) << 12) | (((unsigned char)text[i+1] & 0x3F) << 6) | ((unsigned char)text[i+2] & 0x3F); len = 3; }
        else if ((c1 & 0xF8) == 0xF0 && i + 3 < text.size()) { cp = ((c1 & 0x07) << 18) | (((unsigned char)text[i+1] & 0x3F) << 12) | (((unsigned char)text[i+2] & 0x3F) << 6) | (((unsigned char)text[i+3] & 0x3F)); len = 4; }
        cps.push_back(cp); i += len;
    }
    for (size_t i = 0; i < cps.size(); ++i) {
        float t = (cps.size() > 1) ? (float)i / (cps.size() - 1) : 0;
        uint8_t r = (uint8_t)(r1 + t * (r2 - r1)), g = (uint8_t)(g1 + t * (g2 - g1)), b = (uint8_t)(b1 + t * (b2 - b1));
        set_cell(x + (int)i, y, cps[i], r, g, b, 0, 0, 0, true);
    }
}

void TerminalScreen::draw_box(int x, int y, int w, int h, std::string_view title) {
    if (w < 2 || h < 2) return;
    set_cell(x, y, U'╭'); set_cell(x + w - 1, y, U'╮'); set_cell(x, y + h - 1, U'╰'); set_cell(x + w - 1, y + h - 1, U'╯');
    for (int i = 1; i < w - 1; ++i) { set_cell(x + i, y, U'─'); set_cell(x + i, y + h - 1, U'─'); }
    for (int i = 1; i < h - 1; ++i) { set_cell(x, y + i, U'│'); set_cell(x + w - 1, y + i, U'│'); }
    if (!title.empty()) write_text(x + 2, y, std::format(" {} ", title), 139, 233, 253, 0, 0, 0, true);
}

} // namespace euxis::cli::terminal
