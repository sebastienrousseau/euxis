/// @file
/// @brief Implementation of the ERC-8004 Agent Identity Card standard.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::identity {

/// @brief standardized agent identity card containing capabilities and metadata.
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

/// @brief Factory function to generate an ERC-8004 compliant card.
[[nodiscard]] auto generate_agent_card(
    std::string_view agent_id,
    std::string_view name,
    std::string_view description,
    std::vector<std::string> capabilities,
    std::string_view version = "1.0.0"
) -> ERC8004AgentCard;

} // namespace euxis::identity
