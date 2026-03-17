#pragma once

#include "euxis/cli/tui/widget.hpp"
#include "euxis/cli/tui/color_system.hpp"

#include <functional>
#include <string>
#include <vector>

namespace euxis::cli::tui {

/// A single entry in the command palette.
struct PaletteEntry {
    std::string label;       // Display text
    std::string category;    // Category (e.g., "Command", "Agent", "Model")
    std::string description; // Short description
    int score{0};            // Fuzzy match score (higher is better)
};

/// Fuzzy match scoring algorithm.
struct FuzzyMatch {
    /// Score a candidate string against a query.
    /// Returns 0 if no match, higher scores for better matches.
    [[nodiscard]] static int score(std::string_view query, std::string_view candidate);

    /// Returns the indices of matched characters for highlighting.
    [[nodiscard]] static std::vector<size_t> match_positions(std::string_view query, std::string_view candidate);
};

/// Floating command palette widget (Ctrl+K / Ctrl+P).
/// Provides fuzzy-filtered access to commands, agents, models, and actions.
class Palette : public Widget {
public:
    using SelectCallback = std::function<void(const PaletteEntry&)>;

    Palette();

    /// Set the full list of entries to filter from.
    void set_entries(std::vector<PaletteEntry> entries);

    /// Set the callback for when an entry is selected.
    void set_on_select(SelectCallback callback) { on_select_ = std::move(callback); }

    /// Set the color system for rendering.
    void set_color_system(const ColorSystem* cs) { color_system_ = cs; }

    /// Clear the current query and reset selection.
    void reset();

    /// Get the current query string.
    [[nodiscard]] const std::string& query() const { return query_; }

    /// Get the filtered entries.
    [[nodiscard]] const std::vector<PaletteEntry>& filtered() const { return filtered_; }

    /// Get the selected index.
    [[nodiscard]] int selected() const { return selected_; }

    // Widget interface
    [[nodiscard]] Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] bool focusable() const override { return true; }

private:
    void refilter();

    std::string query_;
    std::vector<PaletteEntry> all_entries_;
    std::vector<PaletteEntry> filtered_;
    int selected_{0};
    int scroll_offset_{0};
    int max_visible_{10};
    SelectCallback on_select_;
    const ColorSystem* color_system_{nullptr};
};

} // namespace euxis::cli::tui
