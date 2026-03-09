#pragma once

#include "euxis/cli/tui/widget.hpp"
#include "euxis/cli/tui/color_system.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace euxis::cli::tui {

/// A single node in the swarm visualization tree.
struct SwarmNode {
    std::string agent_id;
    std::string status;  // "analyzing", "scanning", "waiting", "done", "error"
    bool active{false};
    bool expanded{true};
    double elapsed_s{0.0};
    std::vector<SwarmNode> children;
};

/// Renders multi-agent orchestration as a live tree.
///
/// Example:
/// ```
/// ╭── council: "API design review"
/// │ ├─ architect [*] analyzing... (3.2s)
/// │ ├─ security  [*] scanning... (2.1s)
/// │ ├─ code      [ ] waiting...
/// │ ╰─ qa        [ ] waiting...
/// ```
class SwarmView : public Widget {
public:
    SwarmView();

    /// Set the root node of the swarm tree.
    void set_root(SwarmNode root) { root_ = std::move(root); }

    /// Set the council/swarm name displayed in the header.
    void set_name(std::string name) { name_ = std::move(name); }

    /// Set color system for rendering.
    void set_color_system(const ColorSystem* cs) { color_system_ = cs; }

    // Widget interface
    Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool focusable() const override { return true; }

private:
    SwarmNode root_;
    std::string name_{"Swarm"};
    const ColorSystem* color_system_{nullptr};

    void render_node(terminal::TerminalScreen& screen, const SwarmNode& node,
                     int x, int& y, int max_y, int depth, bool is_last) const;
    int count_visible_lines(const SwarmNode& node) const;
};

} // namespace euxis::cli::tui
