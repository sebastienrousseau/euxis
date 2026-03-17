/// @file
/// @brief HTTP implementation of the A2A transport protocol.
#pragma once

#include <expected>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "agent_card.hpp"

namespace euxis::a2a {

/// @brief Transport layer using HTTP for agent-to-agent communication.
class HttpA2ATransport {
public:
    /// @brief Construct transport with a target base URL.
    explicit HttpA2ATransport(std::string base_url);

    /// @brief Send a JSON-RPC request over HTTP.
    auto send(std::string_view method, const nlohmann::json& params)
        -> std::expected<nlohmann::json, std::string>;

    /// @brief Attempt to discover an agent card at a URL.
    static auto discover(std::string_view url)
        -> std::expected<AgentCard, std::string>;

private:
    std::string base_url_;
};

} // namespace euxis::a2a
