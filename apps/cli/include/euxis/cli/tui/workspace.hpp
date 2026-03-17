#pragma once

#include "euxis/cli/tui/color_system.hpp"
#include "euxis/cli/tui/layout.hpp"
#include "euxis/cli/tui/widget.hpp"

#include <memory>
#include <string>
#include <vector>

namespace euxis::cli::tui {

/// File tree panel for the left pane.
class FileTree : public Widget {
public:
    struct Entry {
        std::string name;
        bool is_dir{false};
        bool expanded{false};
        int depth{0};
    };

    void set_entries(std::vector<Entry> entries) { entries_ = std::move(entries); }
    void set_color_system(const ColorSystem* cs) { color_system_ = cs; }

    Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool handle_event(const Event& event) override;
    bool focusable() const override { return true; }

private:
    std::vector<Entry> entries_;
    int selected_{0};
    int scroll_{0};
    int visible_rows_{0};
    const ColorSystem* color_system_{nullptr};
};

/// Agents panel showing active agents.
class AgentList : public Widget {
public:
    struct AgentEntry {
        std::string id;
        bool active{false};
    };

    void set_agents(std::vector<AgentEntry> agents) { agents_ = std::move(agents); }
    void set_color_system(const ColorSystem* cs) { color_system_ = cs; }

    Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;

private:
    std::vector<AgentEntry> agents_;
    const ColorSystem* color_system_{nullptr};
};

/// Multi-pane IDE-style workspace layout.
/// Adapts to terminal width:
/// - < 80 cols: single pane (chat only)
/// - 80-120 cols: chat + context sidebar
/// - > 120 cols: file tree + chat + context sidebar
class Workspace : public Widget {
public:
    Workspace();

    /// Set the main chat widget.
    void set_chat(std::shared_ptr<Widget> chat) { chat_ = std::move(chat); }

    /// Set the context sidebar widget.
    void set_sidebar(std::shared_ptr<Widget> sidebar) { sidebar_ = std::move(sidebar); }

    /// Set the file tree widget.
    void set_file_tree(std::shared_ptr<Widget> tree) { file_tree_ = std::move(tree); }

    /// Set color system.
    void set_color_system(const ColorSystem* cs) { color_system_ = cs; }

    /// Toggle sidebar visibility.
    void toggle_sidebar() { sidebar_visible_ = !sidebar_visible_; }

    /// Check if sidebar is visible.
    [[nodiscard]] bool sidebar_visible() const { return sidebar_visible_; }

    /// Get the current layout mode based on terminal width.
    [[nodiscard]] LayoutMode current_mode() const { return mode_; }

    // Widget interface
    Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool handle_event(const Event& event) override;

private:
    std::shared_ptr<Widget> chat_;
    std::shared_ptr<Widget> sidebar_;
    std::shared_ptr<Widget> file_tree_;
    const ColorSystem* color_system_{nullptr};
    bool sidebar_visible_{true};
    LayoutMode mode_{LayoutMode::SinglePane};
};

} // namespace euxis::cli::tui
