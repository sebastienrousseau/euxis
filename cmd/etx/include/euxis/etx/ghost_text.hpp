#pragma once
#include <flat_map>
#include <optional>
#include <string>

namespace euxis::etx {

/// Frequency-based command/text suggestion engine.
///
/// Tracks how often each string is submitted and uses prefix
/// matching with frequency ranking to suggest completions.
class GhostTextEngine {
public:
    /// Record a submitted text (increments frequency).
    void record(const std::string& text);

    /// Suggest a completion for @p prefix (returns full string, not just suffix).
    /// Returns the highest-frequency entry that starts with @p prefix.
    auto suggest(const std::string& prefix) const -> std::optional<std::string>;

    /// Access the frequency map (for persistence).
    auto frequencies() const -> const std::flat_map<std::string, int>&;

    /// Load a pre-existing frequency map (e.g. from config).
    void load(std::flat_map<std::string, int> data);

    /// Clear all recorded entries.
    void clear();

private:
    std::flat_map<std::string, int> freq_;
};

} // namespace euxis::etx
