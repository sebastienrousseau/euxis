#include <euxis/etx/ghost_text.hpp>

namespace euxis::etx {

void GhostTextEngine::record(const std::string& text) {
    if (text.empty()) { return; }
    freq_[text]++;
}

auto GhostTextEngine::suggest(const std::string& prefix) const -> std::optional<std::string> {
    if (prefix.empty()) { return std::nullopt; }

    // Use lower_bound for efficient prefix scan
    auto it = freq_.lower_bound(prefix);
    std::optional<std::string> best;
    int best_freq = 0;

    while (it != freq_.end()) {
        const auto& [key, count] = *it;
        // Check if key still starts with prefix
        if (key.size() < prefix.size() || key.compare(0, prefix.size(), prefix) != 0) {
            break;
        }
        // Skip exact match (no completion needed)
        if (key == prefix) {
            ++it;
            continue;
        }
        if (count > best_freq) {
            best_freq = count;
            best = key;
        }
        ++it;
    }

    return best;
}

auto GhostTextEngine::frequencies() const -> const std::flat_map<std::string, int>& {
    return freq_;
}

void GhostTextEngine::load(std::flat_map<std::string, int> data) {
    freq_ = std::move(data);
}

void GhostTextEngine::clear() {
    freq_.clear();
}

} // namespace euxis::etx
