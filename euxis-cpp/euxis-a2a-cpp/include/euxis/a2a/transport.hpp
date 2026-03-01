#pragma once

#include <expected>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "euxis/a2a/agent_card.hpp"

namespace euxis::a2a {

/// Abstract transport layer for A2A protocol communication.
class A2ATransport {
public:
    virtual ~A2ATransport() = default;

    /// Send a JSON-RPC style request to a remote agent.
    [[nodiscard]] virtual auto send(std::string_view method, const nlohmann::json& params)
        -> std::expected<nlohmann::json, std::string> = 0;

    /// Discover a remote agent by fetching its agent card.
    [[nodiscard]] virtual auto discover(std::string_view url)
        -> std::expected<AgentCard, std::string> = 0;
};

} // namespace euxis::a2a
