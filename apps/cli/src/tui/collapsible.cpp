#include "euxis/cli/tui/collapsible.hpp"

#include <format>
#include <sstream>

namespace euxis::cli::tui {

Collapsible::Collapsible(SectionType type, std::string title, std::string content)
    : type_(type), title_(std::move(title)), content_(std::move(content)) {
    accessible_name = "Collapsible: " + title_;
}

Size Collapsible::preferred_size() const {
    int h = 1; // Header line
    if (expanded_) h += content_lines();
    return {60, h};
}

void Collapsible::render(terminal::TerminalScreen& screen, Rect area) {
    if (area.empty()) return;

    // Determine colors
    uint8_t icon_r = 164, icon_g = 133, icon_b = 221; // default magenta
    uint8_t text_r = 169, text_g = 177, text_b = 214;  // tokyo text
    uint8_t dim_r = 68, dim_g = 75, dim_b = 106;       // tokyo dim

    if (color_system_) {
        auto accent = color_system_->color(SemanticRole::Accent);
        icon_r = accent.r; icon_g = accent.g; icon_b = accent.b;
        auto primary = color_system_->color(SemanticRole::Primary);
        text_r = primary.r; text_g = primary.g; text_b = primary.b;
        auto muted = color_system_->color(SemanticRole::Muted);
        dim_r = muted.r; dim_g = muted.g; dim_b = muted.b;
    }

    // Header line
    std::string icon = expanded_ ? "▾ " : "▸ ";
    std::string header;

    switch (type_) {
        case SectionType::Thinking:
            if (active_) {
                header = std::format("{}Thinking... ({}s)", icon, elapsed_.count());
            } else {
                header = std::format("{}Thinking ({}s)", icon, elapsed_.count());
            }
            break;
        case SectionType::ToolUse:
            header = std::format("{}Tool: {}", icon, title_);
            break;
        case SectionType::Context:
            header = std::format("{}Context: {}", icon, title_);
            break;
    }

    screen.write_text(area.x, area.y, header, icon_r, icon_g, icon_b, 0, 0, 0, focused_);

    // Content (when expanded)
    if (expanded_ && area.h > 1) {
        std::istringstream stream(content_);
        std::string line;
        int y = area.y + 1;
        while (std::getline(stream, line) && y < area.y + area.h) {
            std::string prefix = " ┃  ";
            if (static_cast<int>(prefix.size() + line.size()) > area.w) {
                line = line.substr(0, std::max(0, area.w - static_cast<int>(prefix.size())));
            }
            screen.write_text(area.x, y, prefix, dim_r, dim_g, dim_b);
            screen.write_text(area.x + static_cast<int>(prefix.size()), y, line, text_r, text_g, text_b);
            y++;
        }
    }
}

bool Collapsible::handle_event(const Event& event) {
    if (event.type != EventType::Key) return false;

    // Enter or space toggles
    if (event.key == '\r' || event.key == '\n' || event.key == ' ') {
        toggle();
        return true;
    }
    return false;
}

int Collapsible::content_lines() const {
    if (content_.empty()) return 0;
    int count = 1;
    for (char c : content_) {
        if (c == '\n') count++;
    }
    return count;
}

} // namespace euxis::cli::tui
