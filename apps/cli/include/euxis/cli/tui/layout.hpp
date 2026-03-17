#pragma once

#include "euxis/cli/tui/widget.hpp"

#include <memory>
#include <vector>

namespace euxis::cli::tui {

/// Adaptive layout breakpoints.
enum class LayoutMode {
    SinglePane,   // < 80 cols: chat only
    DualPane,     // 80-120 cols: chat + sidebar
    TriplePane,   // > 120 cols: file tree + chat + sidebar
};

/// Determine layout mode from terminal width.
[[nodiscard]] inline LayoutMode layout_mode_for_width(int cols) {
    if (cols > 120) return LayoutMode::TriplePane;
    if (cols >= 80) return LayoutMode::DualPane;
    return LayoutMode::SinglePane;
}

/// Vertical box layout: stacks children top-to-bottom.
class VBox : public Widget {
public:
    void add(std::shared_ptr<Widget> child);
    void clear();
    [[nodiscard]] const std::vector<std::shared_ptr<Widget>>& children() const { return children_; }

    [[nodiscard]] Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool handle_event(const Event& event) override;

private:
    std::vector<std::shared_ptr<Widget>> children_;
};

/// Horizontal box layout: places children left-to-right.
class HBox : public Widget {
public:
    void add(std::shared_ptr<Widget> child);
    void clear();
    [[nodiscard]] const std::vector<std::shared_ptr<Widget>>& children() const { return children_; }

    [[nodiscard]] Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool handle_event(const Event& event) override;

private:
    std::vector<std::shared_ptr<Widget>> children_;
};

/// Horizontal split: two children with a configurable ratio.
class HSplit : public Widget {
public:
    HSplit(std::shared_ptr<Widget> left, std::shared_ptr<Widget> right, float ratio = 0.5f);

    void set_ratio(float r) { ratio_ = r; }
    [[nodiscard]] float ratio() const { return ratio_; }

    [[nodiscard]] Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool handle_event(const Event& event) override;

    [[nodiscard]] const std::shared_ptr<Widget>& left() const { return left_; }
    [[nodiscard]] const std::shared_ptr<Widget>& right() const { return right_; }

private:
    std::shared_ptr<Widget> left_;
    std::shared_ptr<Widget> right_;
    float ratio_;
};

/// Vertical split: two children with a configurable ratio.
class VSplit : public Widget {
public:
    VSplit(std::shared_ptr<Widget> top, std::shared_ptr<Widget> bottom, float ratio = 0.5f);

    void set_ratio(float r) { ratio_ = r; }
    [[nodiscard]] float ratio() const { return ratio_; }

    [[nodiscard]] Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool handle_event(const Event& event) override;

    [[nodiscard]] const std::shared_ptr<Widget>& top() const { return top_; }
    [[nodiscard]] const std::shared_ptr<Widget>& bottom() const { return bottom_; }

private:
    std::shared_ptr<Widget> top_;
    std::shared_ptr<Widget> bottom_;
    float ratio_;
};

/// Float overlay: renders a child centered as a floating panel.
class Float : public Widget {
public:
    explicit Float(std::shared_ptr<Widget> content, float width_pct = 0.6f, int max_rows = 12);

    void set_content(std::shared_ptr<Widget> content) { content_ = std::move(content); }
    void set_width_pct(float pct) { width_pct_ = pct; }
    void set_max_rows(int rows) { max_rows_ = rows; }

    [[nodiscard]] Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] bool focusable() const override { return true; }

private:
    std::shared_ptr<Widget> content_;
    float width_pct_;
    int max_rows_;
};

} // namespace euxis::cli::tui
