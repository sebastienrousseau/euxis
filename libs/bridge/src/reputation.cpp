#include <euxis/bridge/reputation.hpp>

#include <chrono>
#include <format>

namespace euxis::bridge {

namespace {
auto now_iso8601() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time_t, &tm);
    char buf[32];
    (void) std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}
}  // namespace

void ReputationStore::update(const std::string& author_id, double score) {
    auto& rep = get_or_create(author_id);
    rep.score = std::clamp(score, 0.0, 1.0);
    rep.last_updated = now_iso8601();
}

void ReputationStore::flag(const std::string& author_id) {
    auto& rep = get_or_create(author_id);
    rep.skills_flagged++;
    // Decay score on flag
    rep.score = std::max(0.0, rep.score - 0.1);
    rep.last_updated = now_iso8601();
}

void ReputationStore::publish(const std::string& author_id) {
    auto& rep = get_or_create(author_id);
    rep.skills_published++;
    rep.last_updated = now_iso8601();
}

auto ReputationStore::get(const std::string& author_id) const
    -> std::optional<AuthorReputation> {
    auto it = store_.find(author_id);
    if (it != store_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ReputationStore::score(const std::string& author_id) const -> double {
    auto it = store_.find(author_id);
    if (it != store_.end()) {
        return it->second.score;
    }
    return 0.5;  // Default score for unknown authors
}

auto ReputationStore::get_or_create(const std::string& author_id)
    -> AuthorReputation& {
    auto it = store_.find(author_id);
    if (it != store_.end()) {
        return it->second;
    }
    auto& rep = store_[author_id];
    rep.author_id = author_id;
    rep.score = 0.5;
    rep.skills_published = 0;
    rep.skills_flagged = 0;
    rep.last_updated = now_iso8601();
    return rep;
}

}  // namespace euxis::bridge
