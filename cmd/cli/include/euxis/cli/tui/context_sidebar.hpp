#pragma once

#include "euxis/cli/tui/widget.hpp"
#include "euxis/cli/tui/color_system.hpp"

#include <string>
#include <vector>

namespace euxis::cli::tui {

/// Live context sidebar showing session metadata.
/// Auto-refreshes via EventLoop timer.
class ContextSidebar : public Widget {
public:
    ContextSidebar();

    /// Set git branch name.
    void set_git_branch(std::string branch) { git_branch_ = std::move(branch); }

    /// Set modified file count.
    void set_modified_count(int count) { modified_count_ = count; }

    /// Set active model name.
    void set_model(std::string model) { model_ = std::move(model); }

    /// Set active provider name.
    void set_provider(std::string provider) { provider_ = std::move(provider); }

    /// Set estimated token count.
    void set_token_count(int count) { token_count_ = count; }

    /// Set estimated cost.
    void set_cost(double cost) { cost_ = cost; }

    /// Set active agent name.
    void set_agent(std::string agent) { agent_ = std::move(agent); }

    /// Set MCP tools list.
    void set_mcp_tools(std::vector<std::string> tools) { mcp_tools_ = std::move(tools); }

    /// Set color system for rendering.
    void set_color_system(const ColorSystem* cs) { color_system_ = cs; }

    /// Refresh git and system context (git branch, modified files, memory).
    void refresh_system_context();

    // Widget interface
    Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;

private:
    std::string git_branch_;
    int modified_count_{0};
    std::string model_;
    std::string provider_;
    int token_count_{0};
    double cost_{0.0};
    std::string agent_;
    std::vector<std::string> mcp_tools_;
    const ColorSystem* color_system_{nullptr};
};

} // namespace euxis::cli::tui
