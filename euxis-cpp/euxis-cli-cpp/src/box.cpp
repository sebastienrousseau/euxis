#include "euxis/cli/box.hpp"
#include "euxis/cli/term_caps.hpp"

#include <algorithm>
#include <print>
#include <sstream>

namespace euxis::cli::terminal {

namespace {

// Box-drawing characters
struct BoxChars {
    std::string_view tl, tr, bl, br, h, v;
};

auto box_chars() -> BoxChars {
    if (caps().unicode) {
        return {
            "\xe2\x95\xad", "\xe2\x95\xae",  // ╭ ╮
            "\xe2\x95\xb0", "\xe2\x95\xaf",  // ╰ ╯
            "\xe2\x94\x80", "\xe2\x94\x82",  // ─ │
        };
    }
    return {"+", "+", "+", "+", "-", "|"};
}

} // namespace

void print_box(std::string_view title, std::string_view body) {
    auto bc = box_chars();
    int width = caps().cols > 4 ? caps().cols - 4 : 76;

    // Clamp to reasonable range
    auto title_len = static_cast<int>(title.size());
    width = std::max(width, title_len + 4);

    // Top border
    std::string h_line;
    for (int i = 0; i < width; ++i) h_line += bc.h;
    std::println("{}{}{}", bc.tl, h_line, bc.tr);

    // Title
    if (!title.empty()) {
        auto padding = width - title_len;
        std::println("{} {}{}{}", bc.v, bold(title),
                     std::string(padding > 1 ? padding - 1 : 0, ' '), bc.v);

        // Separator
        std::string sep;
        for (int i = 0; i < width; ++i) sep += bc.h;
        std::println("{}{}{}", bc.v, sep, bc.v);
    }

    // Body lines
    std::istringstream stream{std::string{body}};
    std::string line;
    while (std::getline(stream, line)) {
        auto line_len = static_cast<int>(line.size());
        auto pad = width - line_len;
        std::println("{} {}{}{}", bc.v, line,
                     std::string(pad > 1 ? pad - 1 : 0, ' '), bc.v);
    }

    // Bottom border
    std::println("{}{}{}", bc.bl, h_line, bc.br);
}

void print_boxed_table(const std::vector<std::string>& headers,
                       const std::vector<TableRow>& rows) {
    if (headers.empty()) return;

    auto bc = box_chars();

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

    // Total width: sum of columns + 3 chars padding between each + 2 edge padding
    size_t total_inner = 0;
    for (auto w : widths) total_inner += w;
    total_inner += (headers.size() - 1) * 3 + 2;

    // Top border
    std::string h_line;
    for (size_t i = 0; i < total_inner; ++i) h_line += bc.h;
    std::println("{}{}{}", bc.tl, h_line, bc.tr);

    // Header row
    std::print("{} ", bc.v);
    for (size_t i = 0; i < headers.size(); ++i) {
        std::print("{}", bold(headers[i]));
        auto pad = widths[i] - headers[i].size();
        if (i + 1 < headers.size()) {
            std::print("{} {} ", std::string(pad, ' '), bc.v);
        } else {
            std::print("{}", std::string(pad, ' '));
        }
    }
    std::println(" {}", bc.v);

    // Separator
    std::print("{}", bc.v);
    for (size_t i = 0; i < headers.size(); ++i) {
        std::string sep;
        for (size_t j = 0; j < widths[i] + 2; ++j) sep += bc.h;
        std::print("{}", sep);
        if (i + 1 < headers.size()) {
            std::print("+"); // cross junction
        }
    }
    std::println("{}", bc.v);

    // Data rows
    for (const auto& row : rows) {
        std::print("{} ", bc.v);
        for (size_t i = 0; i < headers.size(); ++i) {
            const auto& cell = (i < row.cells.size()) ? row.cells[i] : "";
            std::print("{}", cell);
            auto pad = widths[i] - cell.size();
            if (i + 1 < headers.size()) {
                std::print("{} {} ", std::string(pad, ' '), bc.v);
            } else {
                std::print("{}", std::string(pad, ' '));
            }
        }
        std::println(" {}", bc.v);
    }

    // Bottom border
    std::println("{}{}{}", bc.bl, h_line, bc.br);
}

} // namespace euxis::cli::terminal
