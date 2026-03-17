/// @file
/// @brief Reputation tracking for skill authors.
#pragma once

#include <map>
#include <optional>
#include <string>

namespace euxis::bridge {

/// @brief Reputation metrics for a single author.
struct AuthorReputation {
    std::string author_id;
    double score{0.5};
    int skills_published{0};
    int skills_flagged{0};
    std::string last_updated;
};

/// @brief In-memory store for author reputation data.
class ReputationStore {
public:
    /// @brief Update an author's reputation score.
    void update(const std::string& author_id, double score);
    
    /// @brief Increment the flagged skills count for an author.
    void flag(const std::string& author_id);
    
    /// @brief Increment the published skills count for an author.
    void publish(const std::string& author_id);

    /// @brief Retrieve the full reputation object for an author.
    auto get(const std::string& author_id) const -> std::optional<AuthorReputation>;
    
    /// @brief Get the raw reputation score for an author.
    auto score(const std::string& author_id) const -> double;

private:
    std::map<std::string, AuthorReputation> store_;

    auto get_or_create(const std::string& author_id) -> AuthorReputation&;
};

} // namespace euxis::bridge
