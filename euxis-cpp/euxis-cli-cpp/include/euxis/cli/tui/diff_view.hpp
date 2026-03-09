#pragma once

#include "euxis/cli/tui/widget.hpp"
#include "euxis/cli/tui/color_system.hpp"

#include <string>
#include <vector>

namespace euxis::cli::tui {

/// A single line in a unified diff.
struct DiffLine {
    enum class Type { Context, Addition, Deletion, Header };

    Type type{Type::Context};
    std::string text;
    int old_line{-1};   // Line number in old file (-1 for additions)
    int new_line{-1};   // Line number in new file (-1 for deletions)
};

/// Parsed unified diff.
struct Diff {
    std::string old_file;
    std::string new_file;
    std::vector<DiffLine> lines;
};

/// Parse a unified diff string into structured form.
[[nodiscard]] Diff parse_unified_diff(std::string_view diff_text);

/// Scrollable inline diff rendering widget.
class DiffView : public Widget {
public:
    DiffView();

    /// Set the diff to display.
    void set_diff(Diff diff);

    /// Set the diff from raw unified diff text.
    void set_diff_text(std::string_view text);

    /// Set color system for rendering.
    void set_color_system(const ColorSystem* cs) { color_system_ = cs; }

    /// Get current scroll offset.
    [[nodiscard]] int scroll_offset() const { return scroll_offset_; }

    /// Get total line count.
    [[nodiscard]] int line_count() const { return static_cast<int>(diff_.lines.size()); }

    // Widget interface
    [[nodiscard]] Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] bool focusable() const override { return true; }

private:
    Diff diff_;
    int scroll_offset_{0};
    const ColorSystem* color_system_{nullptr};
};

} // namespace euxis::cli::tui
