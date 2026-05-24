#include "euxis/cli/tui/swarm_view.hpp"

#include <format>

namespace euxis::cli::tui {

SwarmView::SwarmView() {
    accessible_name = "Swarm View";
    accessible_description = "Live visualization of multi-agent orchestration";
}

Size SwarmView::preferred_size() const {
    int lines = 1; // header line
    if (root_.expanded) {
        for (const auto& child : root_.children)
            lines += count_visible_lines(child);
    }
    return {60, lines};
}

void SwarmView::render(terminal::TerminalScreen& screen, Rect area) {
    if (area.empty()) return;

    uint8_t header_r = 164, header_g = 133, header_b = 221;
    if (color_system_) {
        auto c = color_system_->color(SemanticRole::Accent);
        header_r = c.r; header_g = c.g; header_b = c.b;
    }

    // Header
    std::string header = std::format("╭── {}: \"{}\"", root_.agent_id.empty() ? "swarm" : root_.agent_id, name_);
    screen.write_text(area.x, area.y, header, header_r, header_g, header_b, 0, 0, 0, true);

    int y = area.y + 1;
    for (size_t i = 0; i < root_.children.size(); ++i) {
        bool is_last = (i == root_.children.size() - 1);
        render_node(screen, root_.children[i], area.x, y, area.y + area.h, 1, is_last, area.w);
    }
}

void SwarmView::render_node(terminal::TerminalScreen& screen, const SwarmNode& node,
                            int x, int& y, int max_y, int depth, bool is_last, int max_w) const {
    if (y >= max_y) return;

    uint8_t active_r = 56, active_g = 168, active_b = 157;
    uint8_t wait_r = 68, wait_g = 75, wait_b = 106;
    uint8_t text_r = 169, text_g = 177, text_b = 214;
    uint8_t err_r = 247, err_g = 118, err_b = 142;

    if (color_system_) {
        auto a = color_system_->color(SemanticRole::Accent);
        active_r = a.r; active_g = a.g; active_b = a.b;
        auto m = color_system_->color(SemanticRole::Muted);
        wait_r = m.r; wait_g = m.g; wait_b = m.b;
        auto p = color_system_->color(SemanticRole::Primary);
        text_r = p.r; text_g = p.g; text_b = p.b;
        auto e = color_system_->color(SemanticRole::Error);
        err_r = e.r; err_g = e.g; err_b = e.b;
    }

    // Build tree connector
    std::string prefix;
    for (int d = 0; d < depth - 1; ++d) prefix += "│ ";
    prefix += is_last ? "╰─ " : "├─ ";

    // Expansion indicator
    if (!node.children.empty()) {
        prefix += node.expanded ? "[-] " : "[+] ";
    } else {
        prefix += "    ";
    }

    // Agent info
    std::string indicator = node.active ? "[*]" : "[ ]";
    std::string status = node.status;
    if (node.active && node.elapsed_s > 0) {
        status += std::format(" ({:.1f}s)", node.elapsed_s);
    }

    std::string line = std::format("{}{} {} {}", prefix, node.agent_id, indicator, status);
    if (max_w > 0 && static_cast<int>(line.size()) > max_w)
        line = line.substr(0, static_cast<size_t>(max_w));

    // Choose color based on status
    uint8_t r = 0, g = 0, b = 0;
    if (node.status == "error") {
        r = err_r; g = err_g; b = err_b;
    } else if (node.active) {
        r = active_r; g = active_g; b = active_b;
    } else if (node.status == "done") {
        r = text_r; g = text_g; b = text_b;
    } else {
        r = wait_r; g = wait_g; b = wait_b;
    }

    screen.write_text(x, y, line, r, g, b);
    y++;

    // Render children if expanded
    if (node.expanded) {
        for (size_t i = 0; i < node.children.size(); ++i) {
            bool child_last = (i == node.children.size() - 1);
            render_node(screen, node.children[i], x, y, max_y, depth + 1, child_last, max_w);
        }
    }
}

int SwarmView::count_visible_lines(const SwarmNode& node) const {
    int count = 1; // This node
    if (node.expanded) {
        for (const auto& child : node.children) {
            count += count_visible_lines(child);
        }
    }
    return count;
}

} // namespace euxis::cli::tui
