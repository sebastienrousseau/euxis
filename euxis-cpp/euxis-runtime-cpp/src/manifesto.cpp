#include "euxis/runtime/manifesto.hpp"

namespace euxis::runtime {

auto manifesto_to_json(const AgentManifesto& m) -> nlohmann::json {
    nlohmann::json j;

    j["identity"] = {
        {"name", m.identity.name},
        {"role", m.identity.role},
        {"version", m.identity.version},
        {"did", m.identity.did},
        {"description", m.identity.description},
        {"tags", m.identity.tags},
    };

    j["goal"] = {
        {"primary_objective", m.goal.primary_objective},
        {"success_criteria", m.goal.success_criteria},
        {"sub_goals", m.goal.sub_goals},
    };

    nlohmann::json constraints_j;
    constraints_j["constraints"] = nlohmann::json::array();
    for (const auto& c : m.constraints.constraints) {
        constraints_j["constraints"].push_back({
            {"type", c.type},
            {"rule", c.rule},
            {"severity", c.severity},
        });
    }
    constraints_j["allowed_tools"] = m.constraints.allowed_tools;
    constraints_j["forbidden_actions"] = m.constraints.forbidden_actions;
    constraints_j["authorization_scope"] = m.constraints.authorization_scope;
    constraints_j["pii_filtering_required"] = m.constraints.pii_filtering_required;
    j["constraints"] = constraints_j;

    return j;
}

auto manifesto_from_json(const nlohmann::json& j)
    -> std::expected<AgentManifesto, std::string> {
    if (!j.contains("identity") || !j.contains("goal") || !j.contains("constraints")) {
        return std::unexpected("Manifesto must contain identity, goal, and constraints blocks");
    }

    AgentManifesto m;

    const auto& id = j["identity"];
    m.identity.name = id.value("name", "");
    m.identity.role = id.value("role", "");
    m.identity.version = id.value("version", "");
    m.identity.did = id.value("did", "");
    m.identity.description = id.value("description", "");
    m.identity.tags = id.value("tags", std::vector<std::string>{});

    const auto& goal = j["goal"];
    m.goal.primary_objective = goal.value("primary_objective", "");
    m.goal.success_criteria = goal.value("success_criteria", "");
    m.goal.sub_goals = goal.value("sub_goals", std::vector<std::string>{});

    const auto& con = j["constraints"];
    for (const auto& cj : con.value("constraints", nlohmann::json::array())) {
        m.constraints.constraints.push_back({
            .type = cj.value("type", ""),
            .rule = cj.value("rule", ""),
            .severity = cj.value("severity", "medium"),
        });
    }
    m.constraints.allowed_tools = con.value("allowed_tools", std::vector<std::string>{});
    m.constraints.forbidden_actions = con.value("forbidden_actions", std::vector<std::string>{});
    m.constraints.authorization_scope = con.value("authorization_scope", "");
    m.constraints.pii_filtering_required = con.value("pii_filtering_required", true);

    return m;
}

auto validate_manifesto(const AgentManifesto& m)
    -> std::expected<void, std::string> {
    if (m.identity.name.empty()) {
        return std::unexpected("Manifesto identity.name is required");
    }
    if (m.identity.role.empty()) {
        return std::unexpected("Manifesto identity.role is required");
    }
    if (m.goal.primary_objective.empty()) {
        return std::unexpected("Manifesto goal.primary_objective is required");
    }
    return {};
}

} // namespace euxis::runtime
