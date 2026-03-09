#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::identity {

struct ERC8004AgentCard {
    std::string agent_id;
    std::string did;
    std::string name;
    std::string description;
    std::vector<std::string> capabilities;
    std::string version;
    nlohmann::json metadata;

    [[nodiscard]] auto to_json() const -> nlohmann::json;
};

[[nodiscard]] auto generate_agent_card(
    std::string_view agent_id,
    std::string_view name,
    std::string_view description,
    std::vector<std::string> capabilities,
    std::string_view version = "1.0.0"
) -> ERC8004AgentCard;

} // namespace euxis::identity
