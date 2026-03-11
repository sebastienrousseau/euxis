/// @file
/// @brief declarative manifesto for agent behavior and goals.
#pragma once

#include <expected>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::runtime {

/// @brief A behavioral constraint applied to an agent.
struct Constraint {
    std::string type;
    std::string rule;
    std::string severity;
};

/// @brief Unified definition of an agent's identity, goals, and constraints.
struct AgentManifesto {
    /// @brief Core identity information.
    struct Identity {
        std::string name;
        std::string role;
        std::string version;
        std::string did;
        std::string description;
        std::vector<std::string> tags;
    } identity;

    /// @brief primary and secondary goals.
    struct Goal {
        std::string primary_objective;
        std::string success_criteria;
        std::vector<std::string> sub_goals;
    } goal;

    /// @brief Security and operational constraints.
    struct Constraints {
        std::vector<Constraint> constraints;
        std::vector<std::string> allowed_tools;
        std::vector<std::string> forbidden_actions;
        std::string authorization_scope;
        bool pii_filtering_required{true};
    } constraints;
};

/// @brief Serialize manifesto to JSON.
auto manifesto_to_json(const AgentManifesto& m) -> nlohmann::json;

/// @brief parse manifesto from JSON.
auto manifesto_from_json(const nlohmann::json& j)
    -> std::expected<AgentManifesto, std::string>;

/// @brief validate semantic correctness of a manifesto.
auto validate_manifesto(const AgentManifesto& m)
    -> std::expected<void, std::string>;

} // namespace euxis::runtime
