#pragma once

#include "euxis/cli/tui/widget.hpp"
#include "euxis/cli/tui/color_system.hpp"

#include <chrono>
#include <string>

namespace euxis::cli::tui {

/// Collapsible section widget for thinking blocks, tool use, and context.
/// Shows a summary header line that can be expanded to reveal content.
class Collapsible : public Widget {
public:
    /// Section type determines the icon and color scheme.
    enum class SectionType {
        Thinking,  // Spinner while active, timer display
        ToolUse,   // Tool name and output
        Context,   // Context/file listing
    };

    Collapsible(SectionType type, std::string title, std::string content = "");

    /// Set whether the section is expanded.
    void set_expanded(bool expanded) { expanded_ = expanded; }
    [[nodiscard]] bool expanded() const { return expanded_; }

    /// Toggle expanded state.
    void toggle() { expanded_ = !expanded_; }

    /// Set the section content.
    void set_content(std::string content) { content_ = std::move(content); }

    /// Set the title (displayed in collapsed header).
    void set_title(std::string title) { title_ = std::move(title); }

    /// Set elapsed time (for Thinking sections).
    void set_elapsed(std::chrono::seconds elapsed) { elapsed_ = elapsed; }

    /// Set whether this section is actively running.
    void set_active(bool active) { active_ = active; }
    [[nodiscard]] bool active() const { return active_; }

    /// Set color system for rendering.
    void set_color_system(const ColorSystem* cs) { color_system_ = cs; }

    // Widget interface
    [[nodiscard]] Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] bool focusable() const override { return true; }

private:
    SectionType type_;
    std::string title_;
    std::string content_;
    bool expanded_{false};
    bool active_{false};
    std::chrono::seconds elapsed_{0};
    const ColorSystem* color_system_{nullptr};

    [[nodiscard]] int content_lines() const;
};

} // namespace euxis::cli::tui
