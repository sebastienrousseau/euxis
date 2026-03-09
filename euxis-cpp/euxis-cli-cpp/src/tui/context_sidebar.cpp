#include "euxis/cli/tui/context_sidebar.hpp"
#include "euxis/cli/process.hpp"

#include <format>

namespace euxis::cli::tui {

ContextSidebar::ContextSidebar() {
    accessible_name = "Context Sidebar";
    accessible_description = "Shows git status, model info, token count, cost, and active agent";
}

void ContextSidebar::refresh_system_context() {
    // Poll git branch
    auto branch_res = Process::shell("git rev-parse --abbrev-ref HEAD 2>/dev/null");
    if (branch_res.exit_code == 0) {
        git_branch_ = branch_res.stdout_output;
        if (!git_branch_.empty() && git_branch_.back() == '\n') git_branch_.pop_back();
    }

    // Poll modified files count
    auto status_res = Process::shell("git status --porcelain 2>/dev/null | wc -l");
    if (status_res.exit_code == 0) {
        try {
            modified_count_ = std::stoi(status_res.stdout_output);
        } catch (...) {
            modified_count_ = 0;
        }
    }
}

Size ContextSidebar::preferred_size() const {
    return {25, 20};
}

void ContextSidebar::render(terminal::TerminalScreen& screen, Rect area) {
    if (area.empty() || area.w < 10) return;

    uint8_t label_r = 68, label_g = 75, label_b = 106;
    uint8_t value_r = 169, value_g = 177, value_b = 214;
    uint8_t accent_r = 56, accent_g = 168, accent_b = 157;

    if (color_system_) {
        auto m = color_system_->color(SemanticRole::Muted);
        label_r = m.r; label_g = m.g; label_b = m.b;
        auto p = color_system_->color(SemanticRole::Primary);
        value_r = p.r; value_g = p.g; value_b = p.b;
        auto a = color_system_->color(SemanticRole::Accent);
        accent_r = a.r; accent_g = a.g; accent_b = a.b;
    }

    int y = area.y;
    int x = area.x + 1;

    // Title
    screen.write_text(x, y++, "CONTEXT", label_r, label_g, label_b, 0, 0, 0, true);
    y++; // Spacing

    // Git info
    if (!git_branch_.empty()) {
        screen.write_text(x, y, "Git:", label_r, label_g, label_b);
        screen.write_text(x + 5, y, git_branch_, accent_r, accent_g, accent_b);
        y++;

        if (modified_count_ > 0) {
            std::string mod = std::format("M: {} files", modified_count_);
            screen.write_text(x, y, mod, value_r, value_g, value_b);
            y++;
        }
    }

    y++; // Spacing

    // Model info
    if (!model_.empty()) {
        screen.write_text(x, y, "Model:", label_r, label_g, label_b);
        std::string model_display = model_;
        if (static_cast<int>(model_display.size()) > area.w - 8) {
            model_display = model_display.substr(0, area.w - 11) + "...";
        }
        screen.write_text(x + 7, y, model_display, value_r, value_g, value_b);
        y++;
    }

    if (!provider_.empty()) {
        screen.write_text(x, y, "Via:", label_r, label_g, label_b);
        screen.write_text(x + 5, y, provider_, value_r, value_g, value_b);
        y++;
    }

    y++; // Spacing

    // Token / cost info
    if (token_count_ > 0) {
        std::string tok;
        if (token_count_ >= 1000) {
            tok = std::format("Tokens: {:.1f}k", token_count_ / 1000.0);
        } else {
            tok = std::format("Tokens: {}", token_count_);
        }
        screen.write_text(x, y, tok, value_r, value_g, value_b);
        y++;
    }

    if (cost_ > 0.0) {
        std::string cost_str = std::format("Cost: ${:.4f}", cost_);
        screen.write_text(x, y, cost_str, value_r, value_g, value_b);
        y++;
    }

    y++; // Spacing

    // Agent info
    if (!agent_.empty()) {
        screen.write_text(x, y, "Agent:", label_r, label_g, label_b);
        screen.write_text(x + 7, y, agent_, accent_r, accent_g, accent_b);
        y++;
    }

    // MCP tools
    if (!mcp_tools_.empty()) {
        y++; // Spacing
        screen.write_text(x, y, "MCP Tools:", label_r, label_g, label_b);
        y++;
        for (const auto& tool : mcp_tools_) {
            if (y >= area.y + area.h) break;
            screen.write_text(x + 1, y, "- " + tool, value_r, value_g, value_b);
            y++;
        }
    }
}

} // namespace euxis::cli::tui
