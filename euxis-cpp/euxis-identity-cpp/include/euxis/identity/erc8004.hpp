#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::identity {

/// An ERC-8004 Agent Card representing on-chain agent identity metadata.
/// Contains the agent's DID, human-readable information, declared capabilities,
/// and extensible metadata for interoperability with the ERC-8004 registry.
struct ERC8004AgentCard {
    std::string agent_id;
    std::string did;
    std::string name;
    std::string description;
    std::vector<std::string> capabilities;
    std::string version;
    nlohmann::json metadata;

    /// Serialise the agent card to a JSON object conforming to ERC-8004.
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Generate an ERC-8004 compliant agent card for the given agent.
/// The DID is automatically derived as "did:euxis:{agent_id}".
[[nodiscard]] auto generate_agent_card(
    std::string_view agent_id,
    std::string_view name,
    std::string_view description,
    std::vector<std::string> capabilities,
    std::string_view version = "1.0.0"
) -> ERC8004AgentCard;

} // namespace euxis::identity
