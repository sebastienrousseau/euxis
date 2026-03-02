#pragma once

#include <expected>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::runtime {

struct AgentIdentityBlock {
    std::string name;
    std::string role;
    std::string version;
    std::string did;
    std::string description;
    std::vector<std::string> tags;
};

struct AgentGoalBlock {
    std::string primary_objective;
    std::string success_criteria;
    std::vector<std::string> sub_goals;
};

struct AgentConstraint {
    std::string type;     // "hard" or "soft"
    std::string rule;
    std::string severity; // "critical", "high", "medium", "low"
};

struct AgentConstraintsBlock {
    std::vector<AgentConstraint> constraints;
    std::vector<std::string> allowed_tools;
    std::vector<std::string> forbidden_actions;
    std::string authorization_scope;
    bool pii_filtering_required{true};
};

/// IGC (Identity-Goal-Constraints) Agent Manifesto.
struct AgentManifesto {
    AgentIdentityBlock identity;
    AgentGoalBlock goal;
    AgentConstraintsBlock constraints;
};

/// Serialize manifesto to JSON.
[[nodiscard]] auto manifesto_to_json(const AgentManifesto& m) -> nlohmann::json;

/// Deserialize manifesto from JSON.
[[nodiscard]] auto manifesto_from_json(const nlohmann::json& j)
    -> std::expected<AgentManifesto, std::string>;

/// Validate that all required fields are present.
[[nodiscard]] auto validate_manifesto(const AgentManifesto& m)
    -> std::expected<void, std::string>;

} // namespace euxis::runtime
