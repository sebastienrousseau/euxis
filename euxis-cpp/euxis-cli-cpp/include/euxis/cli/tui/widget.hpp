#pragma once

#include "euxis/cli/tui/event_loop.hpp"
#include "euxis/cli/terminal.hpp"

#include <string>

namespace euxis::cli::tui {

/// A rectangular region on screen.
struct Rect {
    int x{0}, y{0}, w{0}, h{0};
    [[nodiscard]] bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
    [[nodiscard]] bool empty() const { return w <= 0 || h <= 0; }
};

/// Preferred size hint from a widget.
struct Size {
    int width{0};
    int height{0};
};

/// Base class for all TUI widgets.
class Widget {
public:
    virtual ~Widget() = default;

    /// Compute the preferred (natural) size of this widget.
    [[nodiscard]] virtual Size preferred_size() const = 0;

    /// Render this widget into the given screen area.
    virtual void render(terminal::TerminalScreen& screen, Rect area) = 0;

    /// Handle an event. Return true if consumed.
    virtual bool handle_event(const Event& event) { (void)event; return false; }

    /// Returns true if this widget can accept keyboard focus.
    [[nodiscard]] virtual bool focusable() const { return false; }

    /// Returns true if this widget currently has focus.
    [[nodiscard]] bool focused() const { return focused_; }
    void set_focused(bool f) { focused_ = f; }

    /// Accessibility: screen reader label.
    std::string accessible_name;
    /// Accessibility: detailed description.
    std::string accessible_description;

    /// Visibility control.
    [[nodiscard]] bool visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }

protected:
    bool focused_{false};
    bool visible_{true};
};

} // namespace euxis::cli::tui
