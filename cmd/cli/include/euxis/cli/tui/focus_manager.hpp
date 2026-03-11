#pragma once

#include "euxis/cli/tui/widget.hpp"

#include <memory>
#include <vector>

namespace euxis::cli::tui {

/// Manages keyboard focus cycling across focusable widgets.
/// Supports Tab/Shift+Tab cycling and modal push/pop for overlays.
class FocusManager {
public:
    /// Register a widget in the focus chain.
    void add(std::shared_ptr<Widget> widget);

    /// Remove a widget from the focus chain.
    void remove(const std::shared_ptr<Widget>& widget);

    /// Clear all widgets from the focus chain.
    void clear();

    /// Move focus to the next focusable widget. Returns true if focus changed.
    bool focus_next();

    /// Move focus to the previous focusable widget. Returns true if focus changed.
    bool focus_prev();

    /// Set focus to a specific widget. Returns true if the widget was found and is focusable.
    bool set_focus(const std::shared_ptr<Widget>& widget);

    /// Returns the currently focused widget (may be null).
    [[nodiscard]] std::shared_ptr<Widget> focused() const;

    /// Returns the index of the currently focused widget (-1 if none).
    [[nodiscard]] int focused_index() const { return current_; }

    /// Push a modal overlay (steals all focus until popped).
    void push_modal(std::shared_ptr<Widget> modal);

    /// Pop the current modal overlay, restoring previous focus.
    void pop_modal();

    /// Returns true if a modal is active.
    [[nodiscard]] bool has_modal() const { return !modal_stack_.empty(); }

    /// Returns the current modal widget (null if none).
    [[nodiscard]] std::shared_ptr<Widget> modal() const;

    /// Number of widgets in the focus chain.
    [[nodiscard]] size_t size() const { return widgets_.size(); }

private:
    std::vector<std::shared_ptr<Widget>> widgets_;
    int current_{-1};
    std::vector<std::shared_ptr<Widget>> modal_stack_;
};

} // namespace euxis::cli::tui
