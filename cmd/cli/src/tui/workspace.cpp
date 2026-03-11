#include "euxis/cli/tui/workspace.hpp"

#include <algorithm>
#include <format>

namespace euxis::cli::tui {

// ---------------------------------------------------------------------------
// FileTree
// ---------------------------------------------------------------------------

Size FileTree::preferred_size() const {
    return {20, static_cast<int>(entries_.size())};
}

void FileTree::render(terminal::TerminalScreen& screen, Rect area) {
    if (area.empty()) return;

    uint8_t text_r = 113, text_g = 153, text_b = 238; // blue
    uint8_t sel_r = 56, sel_g = 168, sel_b = 157;      // cyan
    uint8_t dim_r = 68, dim_g = 75, dim_b = 106;

    if (color_system_) {
        auto c = color_system_->color(SemanticRole::Secondary);
        text_r = c.r; text_g = c.g; text_b = c.b;
        auto a = color_system_->color(SemanticRole::Accent);
        sel_r = a.r; sel_g = a.g; sel_b = a.b;
        auto m = color_system_->color(SemanticRole::Muted);
        dim_r = m.r; dim_g = m.g; dim_b = m.b;
    }

    // Title
    screen.write_text(area.x + 1, area.y, "FILE TREE", dim_r, dim_g, dim_b, 0, 0, 0, true);

    for (int row = 0; row < area.h - 1; ++row) {
        int idx = scroll_ + row;
        if (idx >= static_cast<int>(entries_.size())) break;

        const auto& entry = entries_[idx];
        bool is_sel = (idx == selected_);
        int y = area.y + 1 + row;

        std::string indent(entry.depth * 2, ' ');
        std::string icon = entry.is_dir ? (entry.expanded ? "▾ " : "▸ ") : "  ";
        std::string line = indent + icon + entry.name;

        if (static_cast<int>(line.size()) > area.w) line.resize(static_cast<size_t>(area.w));

        if (is_sel) {
            screen.write_text(area.x, y, line, sel_r, sel_g, sel_b, 26, 27, 42, true);
        } else if (entry.is_dir) {
            screen.write_text(area.x, y, line, text_r, text_g, text_b);
        } else {
            screen.write_text(area.x, y, line, dim_r, dim_g, dim_b);
        }
    }
}

bool FileTree::handle_event(const Event& event) {
    if (event.type != EventType::Key) return false;

    if (event.key == 1001) { // Up
        if (selected_ > 0) selected_--;
        return true;
    }
    if (event.key == 1002) { // Down
        if (selected_ < static_cast<int>(entries_.size()) - 1) selected_++;
        return true;
    }
    if (event.key == '\r' || event.key == '\n') {
        if (selected_ >= 0 && selected_ < static_cast<int>(entries_.size())) {
            if (entries_[selected_].is_dir) {
                entries_[selected_].expanded = !entries_[selected_].expanded;
            }
        }
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// AgentList
// ---------------------------------------------------------------------------

Size AgentList::preferred_size() const {
    return {20, static_cast<int>(agents_.size()) + 1};
}

void AgentList::render(terminal::TerminalScreen& screen, Rect area) {
    if (area.empty()) return;

    uint8_t dim_r = 68, dim_g = 75, dim_b = 106;
    uint8_t text_r = 169, text_g = 177, text_b = 214;
    uint8_t accent_r = 56, accent_g = 168, accent_b = 157;

    if (color_system_) {
        auto m = color_system_->color(SemanticRole::Muted);
        dim_r = m.r; dim_g = m.g; dim_b = m.b;
        auto p = color_system_->color(SemanticRole::Primary);
        text_r = p.r; text_g = p.g; text_b = p.b;
        auto a = color_system_->color(SemanticRole::Accent);
        accent_r = a.r; accent_g = a.g; accent_b = a.b;
    }

    screen.write_text(area.x + 1, area.y, "AGENTS", dim_r, dim_g, dim_b, 0, 0, 0, true);

    for (int i = 0; i < static_cast<int>(agents_.size()) && i + 1 < area.h; ++i) {
        const auto& a = agents_[i];
        std::string line = (a.active ? "[*] " : "[ ] ") + a.id;
        if (static_cast<int>(line.size()) > area.w) line.resize(static_cast<size_t>(area.w));

        if (a.active) {
            screen.write_text(area.x, area.y + 1 + i, line, accent_r, accent_g, accent_b);
        } else {
            screen.write_text(area.x, area.y + 1 + i, line, text_r, text_g, text_b);
        }
    }
}

// ---------------------------------------------------------------------------
// Workspace
// ---------------------------------------------------------------------------

Workspace::Workspace() {
    accessible_name = "Workspace";
    accessible_description = "Multi-pane IDE layout with file tree, chat, and context sidebar";
}

Size Workspace::preferred_size() const {
    return {120, 40};
}

void Workspace::render(terminal::TerminalScreen& screen, Rect area) {
    if (area.empty()) return;

    mode_ = layout_mode_for_width(area.w);

    switch (mode_) {
        case LayoutMode::SinglePane: {
            // Chat only
            if (chat_) chat_->render(screen, area);
            break;
        }
        case LayoutMode::DualPane: {
            // Chat (75%) + Sidebar (25%)
            int sidebar_w = sidebar_visible_ ? area.w / 4 : 0;
            int chat_w = area.w - sidebar_w;

            if (chat_) chat_->render(screen, {area.x, area.y, chat_w, area.h});

            if (sidebar_visible_ && sidebar_) {
                // Draw separator
                for (int y = area.y; y < area.y + area.h; ++y) {
                    screen.set_cell(area.x + chat_w, y, U'│', 68, 75, 106);
                }
                sidebar_->render(screen, {area.x + chat_w + 1, area.y, sidebar_w - 1, area.h});
            }
            break;
        }
        case LayoutMode::TriplePane: {
            // FileTree (20%) + Chat (55%) + Sidebar (25%)
            int tree_w = area.w / 5;
            int sidebar_w = sidebar_visible_ ? area.w / 4 : 0;
            int chat_w = area.w - tree_w - sidebar_w;

            if (file_tree_) {
                file_tree_->render(screen, {area.x, area.y, tree_w - 1, area.h});
                // Separator
                for (int y = area.y; y < area.y + area.h; ++y) {
                    screen.set_cell(area.x + tree_w - 1, y, U'│', 68, 75, 106);
                }
            }

            if (chat_) {
                chat_->render(screen, {area.x + tree_w, area.y, chat_w, area.h});
            }

            if (sidebar_visible_ && sidebar_) {
                int sep_x = area.x + tree_w + chat_w;
                for (int y = area.y; y < area.y + area.h; ++y) {
                    screen.set_cell(sep_x, y, U'│', 68, 75, 106);
                }
                sidebar_->render(screen, {sep_x + 1, area.y, sidebar_w - 1, area.h});
            }
            break;
        }
    }
}

bool Workspace::handle_event(const Event& event) {
    if (chat_ && chat_->handle_event(event)) return true;
    if (sidebar_ && sidebar_->handle_event(event)) return true;
    if (file_tree_ && file_tree_->handle_event(event)) return true;
    return false;
}

} // namespace euxis::cli::tui
