#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace euxis::bridge {

struct AuthorReputation {
    std::string author_id;
    double score;          // 0.0 - 1.0
    uint32_t skills_published;
    uint32_t skills_flagged;
    std::string last_updated;
};

class ReputationStore {
public:
    void update(const std::string& author_id, double score);
    void flag(const std::string& author_id);
    void publish(const std::string& author_id);

    [[nodiscard]] auto get(const std::string& author_id) const
        -> std::optional<AuthorReputation>;

    [[nodiscard]] auto score(const std::string& author_id) const -> double;

private:
    std::unordered_map<std::string, AuthorReputation> store_;
    auto get_or_create(const std::string& author_id) -> AuthorReputation&;
};

}  // namespace euxis::bridge
