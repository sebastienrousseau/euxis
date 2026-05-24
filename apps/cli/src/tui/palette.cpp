#include "euxis/cli/tui/palette.hpp"

#include <algorithm>
#include <cctype>

namespace euxis::cli::tui {

// ---------------------------------------------------------------------------
// FuzzyMatch
// ---------------------------------------------------------------------------

int FuzzyMatch::score(std::string_view query, std::string_view candidate) {
    if (query.empty()) return 1; // Everything matches empty query
    if (candidate.empty()) return 0;

    // Case-insensitive comparison
    auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };

    // Exact prefix match: highest score
    bool prefix = true;
    for (size_t i = 0; i < query.size() && i < candidate.size(); ++i) {
        if (lower(query[i]) != lower(candidate[i])) { prefix = false; break; }
    }
    if (prefix && query.size() <= candidate.size()) return 100;

    // Consecutive character match
    size_t qi = 0;
    int consecutive = 0;
    int max_consecutive = 0;
    int boundary_bonus = 0;

    for (size_t ci = 0; ci < candidate.size() && qi < query.size(); ++ci) {
        if (lower(candidate[ci]) == lower(query[qi])) {
            consecutive++;
            max_consecutive = std::max(max_consecutive, consecutive);

            // Word boundary bonus (after -, _, space, or camelCase)
            if (ci == 0 ||
                candidate[ci - 1] == '-' || candidate[ci - 1] == '_' || candidate[ci - 1] == ' ' ||
                (std::islower(static_cast<unsigned char>(candidate[ci - 1])) &&
                 std::isupper(static_cast<unsigned char>(candidate[ci])))) {
                boundary_bonus += 10;
            }

            qi++;
        } else {
            consecutive = 0;
        }
    }

    if (qi < query.size()) return 0; // Not all query chars matched

    // Score: consecutive bonus + boundary bonus
    int s = 40 + (max_consecutive * 5) + boundary_bonus;
    return std::min(s, 99); // Cap below prefix match
}

std::vector<size_t> FuzzyMatch::match_positions(std::string_view query, std::string_view candidate) {
    std::vector<size_t> positions;
    auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };

    size_t qi = 0;
    for (size_t ci = 0; ci < candidate.size() && qi < query.size(); ++ci) {
        if (lower(candidate[ci]) == lower(query[qi])) {
            positions.push_back(ci);
            qi++;
        }
    }
    return positions;
}

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------

Palette::Palette() {
    accessible_name = "Command Palette";
    accessible_description = "Search and execute commands, switch agents, or change models";
}

void Palette::set_entries(std::vector<PaletteEntry> entries) {
    all_entries_ = std::move(entries);
    refilter();
}

void Palette::reset() {
    query_.clear();
    selected_ = 0;
    refilter();
}

Size Palette::preferred_size() const {
    int h = std::min(max_visible_ + 2, static_cast<int>(filtered_.size()) + 2); // +2 for input line + border
    return {60, h};
}

void Palette::render(terminal::TerminalScreen& screen, Rect area) {
    if (area.empty() || area.w < 10 || area.h < 3) return;

    // Fill background to be opaque
    for (int y = area.y; y < area.y + area.h; ++y) {
        for (int x = area.x; x < area.x + area.w; ++x) {
            screen.set_cell(x, y, U' ', 0, 0, 0, 26, 27, 38);
        }
    }

    int inner_h = area.h;
    int y = area.y;

    // Input line
    std::string prompt = "> " + query_ + "_";
    uint8_t fg_r = 169, fg_g = 177, fg_b = 214; // tokyo_text default
    if (color_system_) {
        auto c = color_system_->color(SemanticRole::Primary);
        fg_r = c.r; fg_g = c.g; fg_b = c.b;
    }
    screen.write_text(area.x, y, prompt, fg_r, fg_g, fg_b, 26, 27, 38, true);
    y++;

    // Separator line
    for (int x = area.x; x < area.x + area.w; ++x) {
        screen.set_cell(x, y, U'─', 68, 75, 106);
    }
    y++;

    // Filtered results (with scroll offset)
    int max_visible_rows = inner_h - 2;
    int visible_count = std::min(static_cast<int>(filtered_.size()) - scroll_offset_, max_visible_rows);
    for (int i = 0; i < visible_count && y < area.y + area.h; ++i) {
        int entry_idx = scroll_offset_ + i;
        const auto& entry = filtered_[entry_idx];
        bool is_sel = (entry_idx == selected_);

        uint8_t text_r = 0, text_g = 0, text_b = 0;
        uint8_t bg_r = 26, bg_g = 27, bg_b = 38;

        if (is_sel) {
            if (color_system_) {
                auto c = color_system_->color(SemanticRole::Accent);
                text_r = c.r; text_g = c.g; text_b = c.b;
            } else {
                text_r = 56; text_g = 168; text_b = 157;
            }
            bg_r = 36; bg_g = 37; bg_b = 52; // Slightly brighter bg for selection
        } else {
            if (color_system_) {
                auto c = color_system_->color(SemanticRole::Secondary);
                text_r = c.r; text_g = c.g; text_b = c.b;
            } else {
                text_r = 113; text_g = 153; text_b = 238;
            }
        }

        // Category tag
        std::string line = "[" + entry.category + "] " + entry.label;
        if (!entry.description.empty()) {
            int remaining = area.w - static_cast<int>(line.size()) - 2;
            if (remaining > 10) {
                std::string desc = entry.description;
                if (static_cast<int>(desc.size()) > remaining) desc = desc.substr(0, remaining - 3) + "...";
                line += "  " + desc;
            }
        }
        if (static_cast<int>(line.size()) > area.w) line = line.substr(0, area.w);

        screen.write_text(area.x, y, line, text_r, text_g, text_b, bg_r, bg_g, bg_b, is_sel);
        y++;
    }
}

bool Palette::handle_event(const Event& event) {
    if (event.type != EventType::Key) return false;

    int key = event.key;

    // Navigation
    if (key == 1001) { // Up
        if (selected_ > 0) selected_--;
        if (selected_ < scroll_offset_) scroll_offset_ = selected_;
        return true;
    }
    if (key == 1002) { // Down
        if (selected_ < static_cast<int>(filtered_.size()) - 1) selected_++;
        if (selected_ >= scroll_offset_ + max_visible_) scroll_offset_ = selected_ - max_visible_ + 1;
        return true;
    }

    // Select
    if (key == '\r' || key == '\n') {
        if (!filtered_.empty() && on_select_) {
            on_select_(filtered_[selected_]);
        }
        return true;
    }

    // Backspace
    if (key == 127 || key == 8) {
        if (!query_.empty()) {
            query_.pop_back();
            refilter();
        }
        return true;
    }

    // Printable character
    if (key >= 32 && key <= 126) {
        query_ += static_cast<char>(key);
        refilter();
        return true;
    }

    return false;
}

void Palette::refilter() {
    scroll_offset_ = 0;
    filtered_.clear();
    for (auto entry : all_entries_) {
        int s = FuzzyMatch::score(query_, entry.label);
        if (s > 0) {
            entry.score = s;
            filtered_.push_back(std::move(entry));
        }
    }

    // Sort by score descending
    std::sort(filtered_.begin(), filtered_.end(), [](const auto& a, const auto& b) {
        return a.score > b.score;
    });

    // Clamp selection
    if (selected_ >= static_cast<int>(filtered_.size())) {
        selected_ = std::max(0, static_cast<int>(filtered_.size()) - 1);
    }
}

} // namespace euxis::cli::tui
