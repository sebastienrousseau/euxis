#include "euxis/identity/erc8004.hpp"

#include <format>

#include <spdlog/spdlog.h>

#include "euxis/identity/did.hpp"

namespace euxis::identity {

auto generate_agent_card(
    std::string_view agent_id,
    std::string_view name,
    std::string_view description,
    std::vector<std::string> capabilities,
    std::string_view version
) -> ERC8004AgentCard {
    const auto did = create_did(agent_id);

    ERC8004AgentCard card{
        .agent_id = std::string{agent_id},
        .did = did,
        .name = std::string{name},
        .description = std::string{description},
        .capabilities = std::move(capabilities),
        .version = std::string{version},
        .metadata = nlohmann::json::object(),
    };

    spdlog::debug("Generated ERC-8004 agent card for '{}' (DID: {})", agent_id, did);
    return card;
}

auto ERC8004AgentCard::to_json() const -> nlohmann::json {
    return nlohmann::json{
        {"agentId", agent_id},
        {"did", did},
        {"name", name},
        {"description", description},
        {"capabilities", capabilities},
        {"version", version},
        {"metadata", metadata},
    };
}

} // namespace euxis::identity
