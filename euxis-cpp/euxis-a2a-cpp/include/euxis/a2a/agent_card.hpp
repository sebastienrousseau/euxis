#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::a2a {

/// Authentication schema for agent discovery.
struct AuthSchema {
    std::string type;  // "bearer", "api_key", "none"
    std::string description;
};

/// A single capability advertised by an agent.
struct Capability {
    std::string name;
    std::string description;
    std::optional<nlohmann::json> input_schema;
    std::optional<nlohmann::json> output_schema;
};

/// An A2A v0.2 Agent Card describing an agent's identity, capabilities,
/// and transport endpoint.
struct AgentCard {
    std::string name;
    std::string description;
    std::string url;
    std::string version;
    std::vector<Capability> capabilities;
    std::optional<AuthSchema> auth;
    std::optional<std::string> identity_did;
    nlohmann::json metadata;

    /// Serialise to JSON using A2A v0.2 camelCase conventions.
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialise from JSON using A2A v0.2 camelCase conventions.
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> AgentCard;
};

/// Validate an agent card according to A2A v0.2 rules.
/// Checks: name non-empty, url non-empty, at least one capability.
[[nodiscard]] auto validate_card(const AgentCard& card) -> std::expected<void, std::string>;

} // namespace euxis::a2a
