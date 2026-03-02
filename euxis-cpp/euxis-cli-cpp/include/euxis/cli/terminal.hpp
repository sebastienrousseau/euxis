#pragma once

#include <cstdint>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "euxis/cli/term_caps.hpp"

namespace euxis::cli::terminal {

// --- Color support ---

/// Returns true if stdout is a terminal and colors should be used.
bool colors_enabled();

/// Returns true if running under CI (GITHUB_ACTIONS, CI, etc.).
bool is_ci();

/// ANSI escape helpers (return empty if colors disabled).
auto bold(std::string_view text) -> std::string;
auto dim(std::string_view text) -> std::string;
auto red(std::string_view text) -> std::string;
auto green(std::string_view text) -> std::string;
auto yellow(std::string_view text) -> std::string;
auto blue(std::string_view text) -> std::string;
auto cyan(std::string_view text) -> std::string;
auto magenta(std::string_view text) -> std::string;

// --- Status icons ---
auto icon_ok() -> std::string;
auto icon_fail() -> std::string;
auto icon_warn() -> std::string;
auto icon_skip() -> std::string;
auto icon_info() -> std::string;

// --- Spinner ---

/// Print a spinner frame to stderr. Call repeatedly with incrementing frame.
void spinner_frame(int frame, std::string_view message);

/// Clear the current spinner line on stderr.
void spinner_clear();

// --- Table ---

struct TableRow {
    std::vector<std::string> cells;
};

/// Print an aligned table to stdout. First row is treated as header.
void print_table(const std::vector<std::string>& headers,
                 const std::vector<TableRow>& rows);

// --- Progress ---

/// Print a progress bar to stderr.
void progress_bar(int current, int total, std::string_view label);

// --- Banner ---

/// Print the Euxis ASCII banner.
void print_banner();

// --- 24-bit color ---

/// Apply 24-bit foreground color (falls back to plain text if truecolor unsupported).
auto rgb_fg(uint8_t r, uint8_t g, uint8_t b, std::string_view text) -> std::string;

/// Apply 24-bit background color (falls back to plain text if truecolor unsupported).
auto rgb_bg(uint8_t r, uint8_t g, uint8_t b, std::string_view text) -> std::string;

// --- Raw Mode & Input ---
void enable_raw_mode();
void disable_raw_mode();
int read_key();

} // namespace euxis::cli::terminal
