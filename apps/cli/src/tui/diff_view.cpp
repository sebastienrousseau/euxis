#include "euxis/cli/tui/diff_view.hpp"

#include <format>
#include <sstream>

namespace euxis::cli::tui {

// ---------------------------------------------------------------------------
// Diff parser
// ---------------------------------------------------------------------------

Diff parse_unified_diff(std::string_view diff_text) {
    Diff diff;
    std::string text_str{diff_text};
    std::istringstream stream{text_str};
    std::string line;

    int old_line = 0, new_line = 0;

    while (std::getline(stream, line)) {
        if (line.starts_with("--- ")) {
            diff.old_file = line.substr(4);
            diff.lines.push_back({DiffLine::Type::Header, line});
        } else if (line.starts_with("+++ ")) {
            diff.new_file = line.substr(4);
            diff.lines.push_back({DiffLine::Type::Header, line});
        } else if (line.starts_with("@@ ")) {
            // Parse hunk header: @@ -old_start,old_count +new_start,new_count @@
            diff.lines.push_back({DiffLine::Type::Header, line});

            // Extract line numbers
            size_t minus = line.find('-');
            size_t plus = line.find('+');
            if (minus != std::string::npos && plus != std::string::npos) {
                try {
                    old_line = std::stoi(line.substr(minus + 1));
                    new_line = std::stoi(line.substr(plus + 1));
                } catch (const std::exception&) { /* swallowed: best-effort path */ (void)0; }
            }
        } else if (line.starts_with("+")) {
            diff.lines.push_back({DiffLine::Type::Addition, line.substr(1), -1, new_line++});
        } else if (line.starts_with("-")) {
            diff.lines.push_back({DiffLine::Type::Deletion, line.substr(1), old_line++, -1});
        } else if (line.starts_with(" ")) {
            diff.lines.push_back({DiffLine::Type::Context, line.substr(1), old_line++, new_line++});
        } else {
            diff.lines.push_back({DiffLine::Type::Context, line, old_line, new_line});
        }
    }

    return diff;
}

// ---------------------------------------------------------------------------
// DiffView
// ---------------------------------------------------------------------------

DiffView::DiffView() {
    accessible_name = "Diff View";
    accessible_description = "Shows file changes in unified diff format";
}

void DiffView::set_diff(Diff diff) {
    diff_ = std::move(diff);
    scroll_offset_ = 0;
}

void DiffView::set_diff_text(std::string_view text) {
    set_diff(parse_unified_diff(text));
}

Size DiffView::preferred_size() const {
    return {80, static_cast<int>(diff_.lines.size()) + 1};
}

void DiffView::render(terminal::TerminalScreen& screen, Rect area) {
    if (area.empty() || diff_.lines.empty()) return;
    last_render_height_ = area.h;

    int line_num_width = 6; // "  123 " format
    int text_start = area.x + line_num_width;
    int text_width = area.w - line_num_width;
    if (text_width < 10) { text_start = area.x; text_width = area.w; line_num_width = 0; }

    for (int row = 0; row < area.h; ++row) {
        int line_idx = scroll_offset_ + row;
        if (line_idx >= static_cast<int>(diff_.lines.size())) break;

        const auto& dl = diff_.lines[line_idx];
        int y = area.y + row;

        uint8_t fg_r = 0, fg_g = 0, fg_b = 0;
        uint8_t bg_r = 0, bg_g = 0, bg_b = 0;

        switch (dl.type) {
            case DiffLine::Type::Addition:
                fg_r = 158; fg_g = 206; fg_b = 106; // green
                bg_r = 15;  bg_g = 30;  bg_b = 15;  // dark green bg
                break;
            case DiffLine::Type::Deletion:
                fg_r = 247; fg_g = 118; fg_b = 142; // red
                bg_r = 35;  bg_g = 12;  bg_b = 15;  // dark red bg
                break;
            case DiffLine::Type::Header:
                fg_r = 113; fg_g = 153; fg_b = 238; // blue
                break;
            case DiffLine::Type::Context:
            default:
                fg_r = 169; fg_g = 177; fg_b = 214; // text
                break;
        }

        if (color_system_) {
            switch (dl.type) {
                case DiffLine::Type::Addition:
                    { auto c = color_system_->color(SemanticRole::Success); fg_r = c.r; fg_g = c.g; fg_b = c.b; }
                    break;
                case DiffLine::Type::Deletion:
                    { auto c = color_system_->color(SemanticRole::Error); fg_r = c.r; fg_g = c.g; fg_b = c.b; }
                    break;
                case DiffLine::Type::Header:
                    { auto c = color_system_->color(SemanticRole::Info); fg_r = c.r; fg_g = c.g; fg_b = c.b; }
                    break;
                case DiffLine::Type::Context:
                    { auto c = color_system_->color(SemanticRole::Primary); fg_r = c.r; fg_g = c.g; fg_b = c.b; }
                    break;
            }
        }

        // Line number
        if (line_num_width > 0 && dl.type != DiffLine::Type::Header) {
            int num = (dl.new_line >= 0) ? dl.new_line : dl.old_line;
            std::string num_str = (num >= 0) ? std::format("{:>4} ", num) : "     ";
            screen.write_text(area.x, y, num_str, 68, 75, 106);
        }

        // Text content
        std::string text = dl.text;
        if (static_cast<int>(text.size()) > text_width) text = text.substr(0, text_width);

        // Indicator column
        char indicator = ' ';
        if (dl.type == DiffLine::Type::Addition) indicator = '+';
        else if (dl.type == DiffLine::Type::Deletion) indicator = '-';

        if (dl.type != DiffLine::Type::Header && line_num_width > 0) {
            screen.set_cell(text_start - 1, y, static_cast<char32_t>(indicator), fg_r, fg_g, fg_b, bg_r, bg_g, bg_b);
        }

        screen.write_text(text_start, y, text, fg_r, fg_g, fg_b, bg_r, bg_g, bg_b);
    }
}

bool DiffView::handle_event(const Event& event) {
    if (event.type != EventType::Key) return false;

    int visible = last_render_height_ > 0 ? last_render_height_ : 10;
    int max_scroll = std::max(0, static_cast<int>(diff_.lines.size()) - visible);

    if (event.key == 1001 || event.key == 'k') { // Up
        if (scroll_offset_ > 0) scroll_offset_--;
        return true;
    }
    if (event.key == 1002 || event.key == 'j') { // Down
        if (scroll_offset_ < max_scroll) scroll_offset_++;
        return true;
    }
    if (event.key == 1012) { // PageUp
        scroll_offset_ = std::max(0, scroll_offset_ - 10);
        return true;
    }
    if (event.key == 1013) { // PageDown
        scroll_offset_ = std::min(max_scroll, scroll_offset_ + 10);
        return true;
    }
    if (event.key == 1010) { // Home
        scroll_offset_ = 0;
        return true;
    }
    if (event.key == 1011) { // End
        scroll_offset_ = max_scroll;
        return true;
    }

    return false;
}

} // namespace euxis::cli::tui
